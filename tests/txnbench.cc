
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <iostream>
#include <fstream>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <chrono>

#include "libroutine.hh"
#undef LOG
#undef ASSERT

#include "conf.h"
#include "debug.h"
#include "statistics.h"
#include "lock.h"
#include "net.h"

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

#define DEADLOCK_TIMEOUT 10000 // us

static int think_time = 0;
static int coroutine_num = 0;

typedef struct {
  uint8_t  action_type; // LOCK or RELEASE
  uint8_t  lock_type; // EXCLUSIVE or SHARED
  uint8_t  txn_type;
  uint32_t txn_id;
  uint32_t lock_id;
  uint64_t timestamp;
  bool granted;
} lock_request;

typedef struct {
  uint32_t txn_id;
  uint32_t txn_type;
  std::vector<lock_request> lock_requests;
  chrono::steady_clock::time_point start_time;
  bool finished;
} transaction;

unordered_map<lktsk, vector<lock_request>::iterator> request_index;
std::vector<transaction> txn_list[DPDK_LCORE_NUM];

int sockfd;
struct sockaddr_in localaddr, remoteaddr;
bool done;

void setup_socket() {
  short port = STARTUP_PORT_BASE + LOCALHOST_ID;

  localaddr.sin_family = AF_INET;
  localaddr.sin_addr.s_addr = INADDR_ANY;
  localaddr.sin_port = htons(port);

  ASSERT((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) >= 0);
  ASSERT(bind(sockfd, (const struct sockaddr *)&localaddr, 
    sizeof(localaddr)) >= 0);
}

void wait_for_startup_packet() {
  char buf[64] = {0};
  socklen_t len;

  LOG("waiting for startup signal...");
  auto ret = recvfrom(sockfd, (char *)buf, sizeof(buf), MSG_WAITALL, 
    (struct sockaddr *)&remoteaddr, &len);
  ASSERT(ret >= 0);

  if (!strncmp((const char *)buf, "start", 5)) {
    timer_start();
    LOG("received startup signal, start requesting for locks.");
  } else {
    close(sockfd);
    ERROR("received wrong signal %s, bailing out.", buf);
  }
}

void wait_for_stop_packet() {
  char buf[64] = {0};
  socklen_t len;

  fprintf(stderr, "waiting for stop signal...\n");
  recvfrom(sockfd, (char *)buf, sizeof(buf), MSG_WAITALL, 
    (struct sockaddr *)&remoteaddr, &len);

  if (!strncmp((const char *)buf, "stop", 4))
    fprintf(stderr, "received stop signal, exiting.\n");
  else {
    close(sockfd);
    ERROR("received wrong signal %s, bailing out.", buf);
  }
}

void sigsegv_handler(int signum, siginfo_t* info, void* context) {
  report_timer();
  report_thpt();
  fflush(stdout);
  fflush(stderr);
  fprintf(stderr, "[host %u] exit\n", LOCALHOST_ID);
  exit(0);
}

void setup_sighandler() {
  struct sigaction act, old_action;
  act.sa_sigaction = sigsegv_handler;
  act.sa_flags = SA_SIGINFO;
  sigemptyset(&(act.sa_mask));
  sigaction(SIGINT, &act, &old_action);
}

int32_t lock_requesting(void* args) {
  uint32_t lcore_id = rte_lcore_id();
  int abort_cnt = 0;
  usleep(100000);

  // We use coroutines to simulate the case that multiple tasks
  // requests the lock in the same time.
  r2::SScheduler ssched;
  for (int c_id = 0; c_id < coroutine_num; c_id++) {
    ssched.spawn([lcore_id, c_id, &abort_cnt]
      (R2_ASYNC) {

      // Prepare to get the throughput.
      auto start_tp = chrono::steady_clock::now();
      const auto timeout = std::chrono::microseconds(DEADLOCK_TIMEOUT);
      uint64_t trace_counter = 0;
      uint64_t txn_counter = 0;

      // Execute each transaction.
      for (int i = c_id; i < txn_list[lcore_id].size(); i += coroutine_num) {
        txn_counter++;
        auto txn = txn_list[lcore_id][i];
        int lr_pos = 0;
        bool abort_flag = false;

        timer_txn_begin(txn.txn_id, txn.txn_type);
        auto stp = chrono::steady_clock::now();

        // Issue all acquire requests.
        for (lr_pos = 0 ; lr_pos < txn.lock_requests.size(); lr_pos++) {
          lock_request lr = txn.lock_requests[lr_pos];
          lock_state op = (lr.lock_type == 1) ? LOCK_SHARED :
                          (lr.lock_type == 2) ? LOCK_EXCL :
                          FREE;

          auto req = lock_acquire_async(lr.lock_id, lr.txn_id, op);

          // Wait until granted or abort.
          // auto stp = chrono::steady_clock::now();
          if (req) {
            while (1) {
              int ret = lock_req_granted(req, lr.lock_id, lr.txn_id);

              if (ret == 2) {
                abort_flag = true;
                break;
              } else if (ret == 1) break;
              else R2_YIELD;

              if (chrono::steady_clock::now() - stp > timeout) {
                abort_flag = true;
                break;
              }
            }
          }

          if (abort_flag == false)
            timer_grant_tx(lr.lock_id, lr.txn_id);

          // If aborted, skip the rest of lock requests.
          if (abort_flag) {
            abort_cnt++;
            txn_counter--;
            break;
          }

          trace_counter++;
        }

        if (!abort_flag && think_time) delay(think_time * 1000);

        // Release locks acquired.
        if (lr_pos == txn.lock_requests.size()) lr_pos -= 1;
        for (; lr_pos >= 0; lr_pos--) {
          lock_request lr = txn.lock_requests[lr_pos];
          lock_state op = (lr.lock_type == 1) ? LOCK_SHARED :
                          (lr.lock_type == 2) ? LOCK_EXCL :
                          FREE;
          lock_release(lr.lock_id, lr.txn_id, op);
        }

        // if (!abort_flag)
        timer_txn_end(txn.txn_id);
      }
      
      // Calculate the throughput.
      auto duration = chrono::duration_cast<chrono::nanoseconds>(
        chrono::steady_clock::now() - start_tp).count();
      txn_thpt_record(lcore_id, c_id, txn_counter, trace_counter, duration);

      // Let the last coroutine stop the scheduler when the trace is
      // completely executed.
      if (c_id == coroutine_num - 1) {
        R2_STOP();
      }
      R2_RET;
    });
  }

  ssched.run();
  fflush(stdout);

  fprintf(stderr, "[host %u core %u] done\n", LOCALHOST_ID, lcore_id);
  fprintf(stderr, "[host %u core %u] abort count %d\n", 
    LOCALHOST_ID, lcore_id, abort_cnt);

  return 0;
}

int32_t rx_loop(void* args) {
  uint32_t lcore_id = rte_lcore_id();

  while (1) {
    size_t rx_cnt = net_poll_packets();
  }
  return 0;
}

// Read a trace file, get all requests for a core.
void get_trace_for_lcores(const char* filename) {
  fprintf(stderr, "Reading trace file %s...", filename);

  ifstream trace_fs;
  trace_fs.open(filename);
  ASSERT(trace_fs.is_open());

  // Read each line.
  string line;
  task_id txn_id = -1;
  while (getline(trace_fs, line)) {
    if (line.c_str()[0] < '0' || line.c_str()[0] > '9')
      continue;
    
    // Get the lock request.
    lock_request lr = {0};
    sscanf(line.c_str(), "%u, %hhu, %hhu, %u, %hhu", &lr.txn_id,
      &lr.action_type, &lr.txn_type, &lr.lock_id, &lr.lock_type);
    
    // If met a new transaction, add it to list.
    int lcore_id = lr.txn_id % TX_CORE_NUM + RX_CORE_NUM;
    if (txn_id != lr.txn_id) {
      transaction txn = {0};
      txn_id = txn.txn_id = lr.txn_id;
      txn.txn_type = lr.txn_type;
      txn_list[lcore_id].push_back(txn);
    }

    txn_list[lcore_id].back().lock_requests.push_back(lr);
    add_txn_lock_map(lr.txn_id, lr.lock_id);
    auto lr_pos = txn_list[lcore_id].back().lock_requests.end() - 1;
    request_index[LKTSK(lr.lock_id, lr.txn_id)] = lr_pos;
  }

  fprintf(stderr, "Reading finished\n");
  trace_fs.close();
}

static int32_t main_loop(__attribute__((unused)) void *arg) {
  uint32_t lcore_id = rte_lcore_id();
  if (lcore_id < RX_CORE_NUM) {
    rx_loop(NULL);
  } else if (lcore_id < RX_CORE_NUM + TX_CORE_NUM) {
    lock_requesting(NULL);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  auto trace_file = getenv(ENV_TRACE_FILE);
  ASSERT_MSG(trace_file, "trace file not provided");

  auto crt_num = getenv(ENV_COROUTINE_NUM);
  if (crt_num) {
    coroutine_num = atoi(crt_num);
  }

  think_time = atoi(getenv(ENV_THINK_TIME));

#ifdef NETLOCK
  if (LOCALHOST_ID == 1) {
    env_setup(argc, argv, 4, 4);
  } else {
    env_setup(argc, argv, 6, 6);
  }
#else
  env_setup(argc, argv, DPDK_LCORE_TX_NUM, DPDK_LCORE_RX_NUM);
  register_flow(SERVER_POST_TYPE, 0, 0);
  register_flow(CLIENT_POST_TYPE, 1, 5);
#endif

  lock_setup(FLAG_ENB_LOCAL_GRANT);
  setup_sighandler();
  setup_socket();
  for (uint32_t lock_id = 0; lock_id < MAX_LOCK_NUM; lock_id++) 
    lock_local_init(lock_id);

  get_trace_for_lcores(trace_file);

  LOG("Coroutine num: %d", coroutine_num);
  LOG("Max lock num: %d", MAX_LOCK_NUM);
  LOG("Think time: %d", think_time);

  wait_for_startup_packet();

  uint32_t lcore_id;
  int ret;
  rte_eal_mp_remote_launch(main_loop, NULL, CALL_MAIN);
  RTE_LCORE_FOREACH_WORKER(lcore_id) {
      if (rte_eal_wait_lcore(lcore_id) < 0) {
          ERROR("lcore %u return error", lcore_id);
          ret = -1;
          break;
      }
  }


  return 0;
}