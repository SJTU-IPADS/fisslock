#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
#include <rte_eal.h>
#include <fstream>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <vector>
#include <chrono>
#include <unistd.h>

#include "request.h"
#include "types.h"
#include "debug.h"
#include "conf.h"
#include "lock.h"
#include "lock_server.h"
#include "statistics.h"

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

// Globals
int sockfd;
struct sockaddr_in localaddr, remoteaddr;
bool map_obj = false;
char *len_in_switch_file = NULL;

// Functions
int32_t main_loop();
int32_t rx_loop(uint32_t lcore_id);
void init_len_in_switch();
void sigsegv_setup();
void sigsegv_handler(int signum, siginfo_t* info, void* context);
void wait_for_startup_packet();
void wait_for_stop_packet() ;
void setup_socket();

void wait_for_startup_packet() {
    char buf[64] = {0};
    socklen_t len;

    fprintf(stderr, "[host%u]waiting for startup signal...\n", LOCALHOST_ID);
    auto ret = recvfrom(sockfd, (char *)buf, sizeof(buf), MSG_WAITALL, 
                        (struct sockaddr *)&remoteaddr, &len);

    if (!strncmp((const char *)buf, "start", 5)) {
        timer_start();
        fprintf(stderr, "[host%u]received startup signal, server started\n", LOCALHOST_ID);
    } else {
        close(sockfd);
        ERROR("received wrong signal %s, bailing out.", buf);
    }
}

void wait_for_stop_packet() {
    char buf[64] = {0};
    socklen_t len;

    fprintf(stderr, "[host%u]waiting for stop signal...\n", LOCALHOST_ID);
    recvfrom(sockfd, (char *)buf, sizeof(buf), MSG_WAITALL, 
                (struct sockaddr *)&remoteaddr, &len);

    if (!strncmp((const char *)buf, "stop", 4)) {
        fprintf(stderr, "[host%u]received stop signal, exiting.\n", LOCALHOST_ID);
    } else {
        close(sockfd);
        ERROR("received wrong signal %s, bailing out.", buf);
    }
}

void setup_socket() {
    short port = STARTUP_PORT_BASE + LOCALHOST_ID;
    // LOG("Socket listen on port %d", port);

    localaddr.sin_family = AF_INET;
    localaddr.sin_addr.s_addr = INADDR_ANY;
    localaddr.sin_port = htons(port);

    ASSERT((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0);
    ASSERT(bind(sockfd, (const struct sockaddr *)&localaddr, 
            sizeof(localaddr)) >= 0);
}


/* 
    Register the handler for SIGSEGV. 
*/
void sigsegv_setup() {
    struct sigaction act, old_action;
    act.sa_sigaction = sigsegv_handler;
    act.sa_flags = SA_SIGINFO;
    sigemptyset(&(act.sa_mask));
    sigaction(SIGINT, &act, &old_action);
}

void sigsegv_handler(int signum, siginfo_t* info, void* context) {
    report_counters();
    report_timer();
    fflush(stderr);
    fflush(stdout);
    fprintf(stderr, "[host%u]exit\n", LOCALHOST_ID);
    exit(0);
}

void init_len_in_switch() {
    len_in_switch = new int[MAX_LOCK_NUM];
    if (!map_obj) {
        for (int i = 0; i < MAX_LOCK_NUM; i++) {
            // TODO: may need to config
            len_in_switch[i] = 10;
        }
    } else {
        std::ifstream len_in_switch_fs;
        len_in_switch_fs.open(len_in_switch_file);
        ASSERT(len_in_switch_fs.is_open());

        std::string line;
        uint32_t lock_id;
        int length;
        while (getline(len_in_switch_fs, line)) {
            sscanf(line.c_str(), "%u,%d\n", &lock_id, &length);
            len_in_switch[lock_id] = length;
        }
        len_in_switch_fs.close();
    }
}

int32_t rx_loop(uint32_t lcore_id) {
    struct lcore_configuration *lconf = &lcore_conf[lcore_id];
    struct rte_mbuf *mbuf_received_burst[DPDK_RX_BURST_SIZE];
    do {
        set_burst_time();
        uint32_t nb_rx = rte_eth_rx_burst(port, lconf->rx_queue_id[0], mbuf_received_burst, DPDK_RX_BURST_SIZE);
        if (nb_rx > 0) {
            for (int i = 0; i < nb_rx; i++) {
                lock_queue_node *node = mbuf_to_node(mbuf_received_burst[i]);
                rte_pktmbuf_free(mbuf_received_burst[i]);
                if (unlikely(node == NULL)) {
                    continue;
                }
                count_rx();
                if (node->role == PRIMARY_BACKUP) {
                    process_primary_backup(lcore_id, node);
                } else if (node->role == SECONDARY_BACKUP) {
                    process_secondary_backup(lcore_id, node);
                } else {
                    free(node);
                }
            }
        }
    } while (1);
}

static int32_t main_loop(__attribute__((unused)) void *arg) {
    uint32_t lcore_id = rte_lcore_id();
    if (lcore_id < RX_CORE_NUM) {
        rx_loop(lcore_id);
    }
    return 0;
}

int main(int argc, char *argv[]) {

    LOCALHOST_ID = atoi(getenv("HOST_ID"));
    map_obj = (bool)(atoi(getenv("MAP_OBJ")));
    len_in_switch_file = getenv("LEN_IN_SWITCH_FILE");
    MAX_LOCK_NUM = atoi(getenv("MAX_LOCK_NUM"));
    LOG("Max lock num: %d", MAX_LOCK_NUM);

#ifdef NETLOCK_READER_FIRST
    LOG("Using reader first policy");
#else
    LOG("Using writer first policy");
#endif

    TX_CORE_NUM = 0;
    RX_CORE_NUM = 8;
    LOG("Server using %d rx cores", RX_CORE_NUM);

    dpdk_setup(argc, argv);

    register_flow(LK_PORT, 0, 7);

    init_all_locks();

    init_len_in_switch();

    sigsegv_setup();

    fflush(stdout);

    uint32_t lcore_id;
    int ret;
    rte_eal_mp_remote_launch(main_loop, NULL, CALL_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            ret = -1;
            break;
        }
    }
    return 0;
}
