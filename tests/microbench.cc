
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
using namespace chrono;
using namespace chrono_literals;

static int think_time = 0;
static int trace_size = 0;
static int coroutine_num = 0;

typedef struct {
  uint8_t  action_type; // LOCK or RELEASE
  uint8_t  lock_type; // EXCLUSIVE or SHARED
  uint8_t  client_id;
  uint32_t txn_id;
  uint32_t lock_id;
  uint64_t timestamp;
} lock_request;

static vector<lock_request> lk_reqs[DPDK_LCORE_NUM];

int sockfd;
struct sockaddr_in localaddr, remoteaddr;

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
  LOG("received SIGINT");
  // report_counters();
  report_timer();
  // report_thpt();
  fflush(stderr);
  fflush(stdout);
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
  double lcore_thpt = 0;
  usleep(100000);

  // We use coroutines to simulate the case that multiple tasks
  // requests the lock in the same time.
  r2::SScheduler ssched;
  for (int c_id = 0; c_id < coroutine_num; c_id++) {
    ssched.spawn([lcore_id, c_id, &lcore_thpt](R2_ASYNC) {

      // Prepare to get the throughput.
      auto start_tp = steady_clock::now();
      uint64_t trace_counter = 0;
      int prev_tick = 0;
      int repeat_time = 0;
      int cur_counter = 0;
      auto prev_tp = start_tp;
      int phase = 0;
      auto phase_start = start_tp + seconds(START_PHASE_POINT_S);
#ifdef RECORD_THPT_IN_TICK
      int i = c_id;
      while (true) {
#else
      for (int i = c_id; i < lk_reqs[lcore_id].size(); i += coroutine_num) {
#endif
#ifdef RECORD_THPT_IN_TICK
        auto cur_tp = steady_clock::now();
        int cur_duration = duration_cast<seconds>(cur_tp - start_tp).count();
        if (unlikely(cur_duration >= THPT_RECORD_DURATION_S)) break;
        if (unlikely(cur_duration >= START_PHASE_POINT_S)) {
          phase = duration_cast<milliseconds>(cur_tp - phase_start).count() / PHASE_DURATION_MS + 1;
        }
        int cur_tick = duration_cast<milliseconds>(cur_tp - start_tp).count() / THPT_RECORD_TICK_MS;
        if (unlikely(cur_tick > prev_tick)) {
            prev_tick = cur_tick;
            auto cur_duration_us = duration_cast<microseconds>(cur_tp - prev_tp).count();
            double cur_thpt = static_cast<double>(cur_counter) / static_cast<double>(cur_duration_us);
            dynamic_thpt_record(lcore_id, cur_tick, cur_thpt);
            prev_tp = cur_tp;
            cur_counter = 0;
        }
#endif
        trace_counter++;
        cur_counter++;

        // Analyze the lock request mode.
        lock_request lr = lk_reqs[lcore_id][i];

#ifdef RECORD_THPT_IN_TICK
        lr.txn_id += (repeat_time * trace_size);
        lr.lock_id = (lr.lock_id + phase * PHASE_LOCK_NUM) % MAX_LOCK_NUM;
#endif

        lock_state op = (lr.lock_type == 1) ? LOCK_SHARED :
                        (lr.lock_type == 2) ? LOCK_EXCL :
                        FREE;

        // We ignore unlock instructions in the microbenchmark.
        if (lr.action_type == 0) {

          // Issue the acquire request.
          uint64_t lktsk = LKTSK(lr.lock_id, lr.txn_id);
          auto req = lock_acquire_async(lr.lock_id, lr.txn_id, op);

          // Wait until the request is fulfilled.
          if (req) while (1) {
            int ret = lock_req_granted(req, lr.lock_id, lr.txn_id);

            // Aborted, re-acquire.
            if (ret == 2) req = lock_acquire_async(lr.lock_id, lr.txn_id, op);
            else if (ret == 1) break;
            else R2_YIELD;
          } 

          if (think_time) delay(think_time * 1000);

          // Release the lock.
          lock_release(lr.lock_id, lr.txn_id, op);
        }
#ifdef RECORD_THPT_IN_TICK
        i += coroutine_num;
        if (unlikely(i >= lk_reqs[lcore_id].size())) {
            i = c_id;
            repeat_time++;
        }
#endif
      }
      
      // Calculate the throughput.
      auto duration = chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now() - start_tp).count();
      lcore_thpt += ((double)trace_counter / (double)duration);

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

  fprintf(stderr, "[host%u]tx core %u done, request throughput %.4f\n", LOCALHOST_ID, lcore_id, lcore_thpt);

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
  LOG("Reading trace file %s", filename);

  ifstream trace_fs;
  trace_fs.open(filename);
  ASSERT(trace_fs.is_open());

  // Read each line.
  string line;
  int tx_lcore_id = 0;
  while (getline(trace_fs, line)) {
    if (line.c_str()[0] < '0' || line.c_str()[0] > '9')
      continue;

    lock_request lr;
    sscanf(line.c_str(), "%u, %hhu, %hhu, %u, %hhu", &lr.txn_id,
      &lr.action_type, &lr.client_id, &lr.lock_id, &lr.lock_type);
    lr.lock_id = lr.lock_id % MAX_LOCK_NUM;

#ifdef FALLBACK_TO_PARLOCK
    lr.lock_id += SWITCH_LOCK_NUM;
#endif

    lk_reqs[RX_CORE_NUM + tx_lcore_id].push_back(lr);
    trace_size++;
    tx_lcore_id = (tx_lcore_id + 1) % TX_CORE_NUM;
  }

  for (int core = RX_CORE_NUM; core < RX_CORE_NUM + TX_CORE_NUM; core++) {
    vector<lock_request> v = lk_reqs[core];
    for (int i = 0; i < 2; i++)
      lk_reqs[core].insert(lk_reqs[core].end(), v.begin(), v.end());
  }

  LOG("Reading finished");
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
    register_flow(LK_PORT, 0, 3);
  } else {
    env_setup(argc, argv, 6, 6);
    register_flow(LK_PORT, 0, 5);
  }
#else
  env_setup(argc, argv, DPDK_LCORE_TX_NUM, DPDK_LCORE_RX_NUM);
  register_flow(SERVER_POST_TYPE, 0, 0);
  register_flow(CLIENT_POST_TYPE, 1, 5);
#endif

  LOG("Coroutine num: %d", coroutine_num);
  LOG("Max lock num: %d", MAX_LOCK_NUM);
  LOG("Think time: %d", think_time);

  lock_setup(FLAG_ENB_LOCAL_GRANT);
  setup_sighandler();
  setup_socket();

  auto lnum = MAX_LOCK_NUM;
#ifdef FALLBACK_TO_PARLOCK
  lnum += SWITCH_LOCK_NUM;
#endif
  for (uint32_t lock_id = 0; lock_id < lnum; lock_id++) 
    lock_local_init(lock_id);

  get_trace_for_lcores(trace_file);

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