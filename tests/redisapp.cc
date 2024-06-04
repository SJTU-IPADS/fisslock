
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <hiredis.h>
#include <async.h>
#include <adapters/poll.h>

#include <redlock-cpp/redlock.h>

#include <vector>
#include <unordered_map>
#include <chrono>

#include "libroutine.hh"
#undef LOG
#undef ASSERT

#include "conf.h"
#include "debug.h"
#include "statistics.h"
#include "lock.h"
#include "net.h"
#include "random.h"

using namespace std;
using namespace std::chrono;
using namespace std::chrono_literals;

#define DEADLOCK_TIMEOUT 10000 // us
#define THPT_INTERVAL    5000 // ms

// #define REDLOCK

static int coroutine_num = 0;

typedef enum {
  READ,
  READ_MODIFY_WRITE,
  UPDATE,
  DELETE,
} txn_type;

const char* redis_servers[] = {
  "192.168.12.47",
  "192.168.12.138",
  "192.168.12.9",
  "192.168.12.97",
};

#define REDIS_SERVER_NUM (sizeof(redis_servers) / sizeof(char*))

#ifdef REDLOCK

#define LOCK_PREPARE(lock_num) CLock mtxes[lock_num]

#define LOCK_ACQUIRE(lock, txn, op) do {\
  char lock_name[16];\
  snprintf(lock_name, 16, "lock-%d", lock);\
  while (!lock_srv->Lock(lock_name, 10000, mtxes[idx])) {\
    R2_YIELD;\
    if (chrono::steady_clock::now() - stp > timeout) {\
      abort_flag = true;\
      break;\
    }\
  }\
  if (!abort_flag) timer_grant_tx(lock, txn);\
  else {\
    abort_cnt++;\
    break;\
  }\
} while (0)

#define LOCK_RELEASE(lock, txn, op) do {\
  lock_srv->Unlock(mtxes[idx]);\
} while (0)

#else

#define LOCK_PREPARE(lock_num) (0)

#define LOCK_ACQUIRE(lock, txn, op) do {\
  auto req = lock_acquire_async(lock, txn, op);\
  if (req) while (1) {\
    int ret = lock_req_granted(req, lock, txn);\
    if (ret == 1) break; else R2_YIELD;\
    if (chrono::steady_clock::now() - stp > timeout) {\
      abort_flag = true;\
      break;\
    }\
  }\
  if (!abort_flag) timer_grant_tx(lock, txn);\
  else {\
    abort_cnt++;\
    break;\
  }\
} while (0)

#define LOCK_RELEASE(lock, txn, op) lock_release(lock, txn, op)

#endif

uint32_t txn_counter[DPDK_LCORE_NUM];
double txn_thpt_percore[DPDK_LCORE_NUM];

void sigsegv_handler(int signum, siginfo_t* info, void* context) {
  report_timer();
  // report_thpt();
  double thpt = 0;
  for (int i = 0; i < DPDK_LCORE_NUM; i++) thpt += txn_thpt_percore[i];
  LOG("[host %u] txn throughput %.2f Kps", LOCALHOST_ID, thpt);

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

void redis_command_cb(redisAsyncContext* c, void* reply, void* privdata) {
  *(bool *)privdata = true;
}

int32_t lock_requesting(void* args) {
  uint32_t lcore_id = rte_lcore_id();
  int abort_cnt = 0;

  // Connect to all redis servers. This must be done thread-wise
  // because the async context is not thread-safe.
  redisAsyncContext* redis_srv[REDIS_SERVER_NUM];
  for (int i = 0; i < REDIS_SERVER_NUM; i++) {
    redis_srv[i] = redisAsyncConnect(redis_servers[i], 6379);
    if (redis_srv[i]->err)
      ERROR("failed to connect to Redis server (%s)", redis_srv[i]->errstr);

    redisPollAttach(redis_srv[i]);
  }

  // Connect to the lock server. This server is only used when we
  // use RedLock for distributed locking.
  CRedLock* lock_srv = new CRedLock();
  lock_srv->AddServerUrl(redis_servers[0], 6379);
  lock_srv->SetRetry(1, 1);

  // Initialize the random number generator.
  auto rand = Random(LOCALHOST_ID * DPDK_LCORE_NUM + lcore_id, 
    0, MAX_LOCK_NUM - 1);
  rand.init_zipfian();

  // We use coroutines to simulate the case that multiple tasks
  // requests the lock in the same time.
  int done_cnt = 0;
  r2::SScheduler ssched;
  for (int c_id = 0; c_id < coroutine_num; c_id++) {
    ssched.spawn(
      [lcore_id, c_id, &abort_cnt, &rand, &redis_srv, &lock_srv, &done_cnt]
      (R2_ASYNC) {

      // Prepare to get the throughput.
      auto start_tp = steady_clock::now();
      const auto timeout = microseconds(DEADLOCK_TIMEOUT);
      const auto thpt_interval = milliseconds(THPT_INTERVAL);

      // Generate and execute transactions continuously.
      // Do not stop until we meet a SIGINT.
      while (!done_cnt) {
        auto txn_id = (LOCALHOST_ID * HOST_NUM + txn_counter[lcore_id]) 
          * DPDK_LCORE_NUM + lcore_id;
        auto txn_tp = (rand.next_uniform_real() > 0.1) ? 
          READ : READ_MODIFY_WRITE;
        // auto txn_tp = READ;
        // LOG("[core%d] start a txn (id %d)", lcore_id, txn_id);

        // Start the execution of the transaction.
        timer_txn_begin(txn_id, txn_tp);
        auto stp = steady_clock::now();
        bool abort_flag = false;
        switch (txn_tp) {

          // OBTAIN BALANCE:
          // Read the account balance of a specific user.
          case READ:
          {
            int idx = 0;
            int user = rand.next_zipfian();

            // Acquire all locks.
            LOCK_PREPARE(1);
            LOCK_ACQUIRE(user, txn_id, LOCK_SHARED);

            // Execute the transaction.
            if (!abort_flag) {
              bool completion = false;
              int srv_i = user % REDIS_SERVER_NUM;
              redisAsyncCommand(redis_srv[srv_i], redis_command_cb, 
                (void *)&completion, "HGET %d balance", user);

              while (!completion) {
                R2_YIELD;
                redisPollTick(redis_srv[srv_i], 0);
              }
            }

            // Release all locks.
            if (!abort_flag)
              LOCK_RELEASE(user, txn_id, LOCK_SHARED);

            break;
          }

          // TRANSFER MONEY:
          // Reads two records, subtracts x$ from the one of the two 
          // and adds x$ to the other before writing them both back.
          case READ_MODIFY_WRITE:
          {
            int idx = 0;
            int users[2] = {rand.next_zipfian(), rand.next_zipfian()};
            if (users[0] == users[1]) users[1]++;
            int bill = rand.next_uniform_real() * 10000;

            LOCK_PREPARE(2);
            for (; idx < 2; idx++) LOCK_ACQUIRE(users[idx], txn_id, LOCK_EXCL);

            if (!abort_flag) {
              bool completion[2] = {0};
              redisAsyncCommand(redis_srv[users[0] % REDIS_SERVER_NUM],
                redis_command_cb, (void *)&completion[0], 
                "HINCRBY %d balance %d", users[0], bill);
              redisAsyncCommand(redis_srv[users[1] % REDIS_SERVER_NUM],
                redis_command_cb, (void *)&completion[1], 
                "HDECRBY %d balance %d", users[1], bill);

              for (int i = 0; i < 2; i++) {
                while (!completion[i]) {
                  R2_YIELD;
                  for (int srv = 0; srv < REDIS_SERVER_NUM; srv++)
                    redisPollTick(redis_srv[srv], 0);
                }
              }
            }

            if (idx == 2) idx--;
            for (; idx >= 0; idx--) {
              LOCK_RELEASE(users[idx], txn_id, LOCK_EXCL);
            }

            break;
          }
        }
        
        if (!abort_flag) txn_counter[lcore_id]++;
        timer_txn_end(txn_id);

        // Calculate the throughput for every fixed time period.
        auto end_tp = steady_clock::now();
        if (end_tp - start_tp > thpt_interval && c_id == 0) {
          auto duration = duration_cast<nanoseconds>(end_tp - start_tp).count();
          txn_thpt_percore[lcore_id] = txn_counter[lcore_id] / 
                                      (double)duration * 1000 * 1000;
          txn_counter[lcore_id] = 0;
          start_tp = end_tp;
          // LOG("[host %d core %d] throughput: %.2f Kps", 
          //   LOCALHOST_ID, lcore_id, txn_thpt_percore[lcore_id]);

          done_cnt++;
          break;
        }
      }

      if (done_cnt++ == coroutine_num) R2_STOP();
      R2_RET;
    });
  }

  ssched.run();
  fflush(stdout);

  fprintf(stderr, "[host %u core %u] done\n", LOCALHOST_ID, lcore_id);
  fprintf(stderr, "[host %u core %u] abort count %d\n", 
    LOCALHOST_ID, lcore_id, abort_cnt);

  for (int srv = 0; srv < REDIS_SERVER_NUM; srv++) {
    // redisPollTick(redis_srv[srv], 0);
    redisAsyncFree(redis_srv[srv]);
  }
  return 0;
}

int32_t rx_loop(void* args) {
  uint32_t lcore_id = rte_lcore_id();

  while (1) {
    size_t rx_cnt = net_poll_packets();
  }
  return 0;
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
  auto crt_num = getenv(ENV_COROUTINE_NUM);
  if (crt_num) {
    coroutine_num = atoi(crt_num);
  }

#ifdef NETLOCK
  if (LOCALHOST_ID == 1) {
    env_setup(argc, argv, 4, 4);
  } else {
    env_setup(argc, argv, 6, 6);
  }
#else
  env_setup(argc, argv, DPDK_LCORE_TX_NUM, DPDK_LCORE_RX_NUM);
  register_flow(SERVER_POST_TYPE, 0, 0);
  register_flow(CLIENT_POST_TYPE, 1, 3);
#endif

  lock_setup(FLAG_ENB_LOCAL_GRANT);
  setup_sighandler();
  for (uint32_t lock_id = 0; lock_id < MAX_LOCK_NUM; lock_id++) 
    lock_local_init(lock_id);

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