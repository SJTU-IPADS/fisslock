
#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <chrono>
#include <cassert>
#include <unistd.h>

#include "statistics.h"
#include "net.h"
#include "conf.h"

using namespace std;
using namespace chrono;
using namespace chrono_literals;

/**
 * Some global utilities.
 */
#define latency(beg, fin) duration_cast<nanoseconds>(fin - beg).count()
#define timestamp(tc) \
  time_point_cast<nanoseconds>(tc).time_since_epoch().count()

/**
 * A map that records the set of locks held by a transaction,
 * used for fixing the latency bias.
 */
unordered_map<uint32_t, vector<uint32_t>> txn_lock_map;
void add_txn_lock_map(uint32_t txn_id, uint32_t lock_id) {
  txn_lock_map[txn_id].push_back(lock_id);
}

/******************************************************
 * Throughput statistics                              *
 ******************************************************/

/**
 * Data structures for measuring the throughput of transaction
 * execution and lock requesting.
 */
typedef struct {
  uint64_t txn_cnt;
  uint64_t request_cnt;
  int64_t nano_duration;
} thpt_info;

unordered_map<uint32_t, thpt_info> txn_thpt_map[DPDK_LCORE_NUM];

void txn_thpt_record(uint32_t lcore_id, uint32_t c_id, 
  uint64_t txn_cnt, uint64_t request_cnt, int64_t nano_duration) {
  thpt_info info = {txn_cnt, request_cnt, nano_duration};
  txn_thpt_map[lcore_id].emplace(c_id, info);
}

/**
 * Data structures for measuring the dynamic lock request throughput.
 * `tick` denotes the logical timestamp and `thpt` denotes the instant
 * throughput at that time.
 */
typedef struct {
  int tick;
  double thpt;
} dynamic_thpt_info;

vector<dynamic_thpt_info> dynamic_thpt[DPDK_LCORE_NUM];

void dynamic_thpt_record(uint32_t core, int tick, double thpt) {
  dynamic_thpt_info info = {tick, thpt};
  dynamic_thpt[core].push_back(info);
}

void report_thpt() {
  for (auto lcore_id = RX_CORE_NUM; 
            lcore_id < RX_CORE_NUM + TX_CORE_NUM; lcore_id++) {
    double lock_thrpt = 0, txn_thrpt = 0;

    for (auto& i : txn_thpt_map[lcore_id]) {
      int64_t extra_time = 0;
      auto duration = duration_cast<microseconds>(
        nanoseconds(i.second.nano_duration - extra_time)).count();
      lock_thrpt += (double)(i.second.request_cnt) / duration;
      txn_thrpt += (double)(i.second.txn_cnt) / duration;
    }

    fprintf(stderr, "[host %u core %u] request throughput %.4f\n", 
      LOCALHOST_ID, lcore_id, lock_thrpt);
    fprintf(stderr, "[host %u core %u] transaction throughput %.4f\n", 
      LOCALHOST_ID, lcore_id, txn_thrpt);
  }

  int total_tick_thpt_count = 0;
  for (int i = RX_CORE_NUM; i < RX_CORE_NUM + TX_CORE_NUM; i++) {
    for (auto& thpt_info : dynamic_thpt[i]) {
      printf("%d,0,%.6f,pt\n", thpt_info.tick, thpt_info.thpt);
    }
    total_tick_thpt_count += dynamic_thpt[i].size();
  }
  fprintf(stderr, "[host %u] total tick thpt count %d\n", 
      LOCALHOST_ID, total_tick_thpt_count);
}

/******************************************************
 * Timer utility: recording timestamps                *
 ******************************************************/

/* Macros for processing timestamps. */
#define t(lktsk, name) (name##_time[0].find(lktsk))
#define t_exist(t, name) (t != name##_time[0].end())
#define t_lcore(lktsk, name, lcore_id) (name##_time[lcore_id].find(lktsk))
#define t_exist_lcore(t, name, lcore_id) (t != name##_time[lcore_id].end())

/**
 * Define a timer with specified name. 
 * 
 * Exposes the timer interface:
 * - timer_xxx(lock, task) create a timestamp for given lock & task.
 */
#define timer_define(name) \
static unordered_map<uint64_t, steady_clock::time_point> \
  name##_time[DPDK_LCORE_NUM]; \
void timer_##name(lock_id lock, task_id task) {\
  auto ret = name##_time[net_lcore_id()].insert(\
    {LKTSK(lock, task), steady_clock::now()});\
}\
void timer_since_burst_##name(lock_id lock, task_id task) {\
  auto ret = name##_time[net_lcore_id()].insert(\
    {LKTSK(lock, task), burst_time[net_lcore_id()]});\
}\

#define timer_print(lktsk, beg, fin, type) do {\
  printf("%lx,0,%ld,%s\n", lktsk, latency(beg, fin), type);\
} while (0)

#define timer_print_host_lcore(lktsk, beg, fin, type, lcore_id) do {\
  printf("%lx,0,%ld,%s,%d,%d\n", lktsk, latency(beg, fin), type, LOCALHOST_ID, lcore_id);\
} while (0)

#define timer_print_from_to(to, from, beg, fin, type) do {\
  printf("%lx,%lx,%ld,%s\n", to, from, latency(beg, fin), type);\
} while (0)

#define timer_merge(name) do {\
  for (size_t _core = 1; _core < DPDK_LCORE_NUM; _core++) {\
    name##_time[0].insert(\
      name##_time[_core].begin(), name##_time[_core].end());\
  }\
} while (0)

static steady_clock::time_point burst_time[DPDK_LCORE_NUM];
void set_burst_time() {
  burst_time[net_lcore_id()] = steady_clock::now();
}

/* Provide interface for checking the current system time. */
static uint64_t start_time;
void timer_start() {
  start_time = timestamp(steady_clock::now());
}

uint64_t timer_now() {
  return timestamp(steady_clock::now()) - start_time;
}

/* Record the transaction latency and type. */
static unordered_map<uint32_t,steady_clock::time_point> 
  txn_begin_time[DPDK_LCORE_NUM], txn_end_time[DPDK_LCORE_NUM];
static unordered_map<uint32_t, uint32_t> txn_type_map[DPDK_LCORE_NUM];

void timer_txn_begin(uint32_t txn_id, uint32_t txn_type) {
  txn_begin_time[net_lcore_id()].insert({txn_id, steady_clock::now()});
  txn_type_map[net_lcore_id()].insert({txn_id, txn_type});
}
void timer_txn_end(uint32_t txn_id) {
  txn_end_time[net_lcore_id()].insert({txn_id, steady_clock::now()});
}

static unordered_map<uint64_t, uint64_t> schedule_pair[DPDK_LCORE_NUM];

timer_define(acquire);
timer_define(acquire_sent);
timer_define(grant_begin);
timer_define(grant_w_agent);
timer_define(grant_wo_agent);
timer_define(grant_local);
timer_define(grant_tx);
timer_define(release);
timer_define(release_sent);
timer_define(release_local);
timer_define(schedule_start);
timer_define(schedule_end);
timer_define(handle_acquire_begin);
timer_define(handle_acquire_end);
timer_define(queue_start);
timer_define(queue_end);
timer_define(secondary_begin);
timer_define(secondary_end);
timer_define(switch_direct_grant);

void report_timer() {
  timer_merge(acquire);
  timer_merge(acquire_sent);
  timer_merge(grant_begin);
  timer_merge(grant_w_agent);
  timer_merge(grant_wo_agent);
  timer_merge(grant_local);
  timer_merge(grant_tx);
  timer_merge(release);
  timer_merge(release_sent);
  timer_merge(release_local);
  timer_merge(schedule_start);
  timer_merge(schedule_end);
  timer_merge(txn_begin);
  timer_merge(txn_end);
  timer_merge(queue_start);
  timer_merge(queue_end);
  timer_merge(secondary_begin);
  timer_merge(secondary_end);
  timer_merge(switch_direct_grant);

  for (int i = 0; i < DPDK_LCORE_NUM; i++) {
    schedule_pair[0].insert(schedule_pair[i].begin(), schedule_pair[i].end());
    txn_type_map[0].insert(txn_type_map[i].begin(), txn_type_map[i].end());
  }

  for (int lcore_id = 0; lcore_id < DPDK_LCORE_NUM; lcore_id++) {
    for (auto const &i : handle_acquire_begin_time[lcore_id]) {
      auto lktsk = i.first;
      auto hab = i.second;
      auto hae = t_lcore(lktsk, handle_acquire_end, lcore_id);
      if (t_exist_lcore(hae, handle_acquire_end, lcore_id)) {
        timer_print_host_lcore(lktsk, hab, hae->second, "h", lcore_id);
      }
    }
  }
  for (auto const &i : acquire_time[0]) {
    auto lktsk = i.first;
    auto at = i.second;

    // acquire call -> acquire request sent
    auto ast = t(lktsk, acquire_sent);
    assert(t_exist(ast, acquire_sent));
    timer_print(lktsk, at, ast->second, "a");

    // grant reply received -> granted
    auto gbt = t(lktsk, grant_begin);
    if (t_exist(gbt, grant_begin)) {
      auto glt = t(lktsk, grant_local);
      auto gwt = t(lktsk, grant_w_agent);
      auto gwot = t(lktsk, grant_wo_agent);
      auto gst = t(lktsk, switch_direct_grant);
      if (t_exist(glt, grant_local)) {
        timer_print(lktsk, gbt->second, glt->second, "gl");
        timer_print(lktsk, at, glt->second, "ge");
      } else if (t_exist(gwt, grant_w_agent)) {
        timer_print(lktsk, gbt->second, gwt->second, "gw");
        timer_print(lktsk, at, gwt->second, "ge");
      } else if (t_exist(gwot, grant_wo_agent)) {
        timer_print(lktsk, gbt->second, gwot->second, "gn");
        timer_print(lktsk, at, gwot->second, "ge");
      } else if (t_exist(gst, switch_direct_grant)) {
        timer_print(lktsk, gbt->second, gst->second, "gs");
        timer_print(lktsk, at, gst->second, "ge");
      } else {
        /* error */
        assert(0);
      }
    }

    // release call -> release request sent
    auto rt = t(lktsk, release);
    if (t_exist(rt, release)) {
      auto rst = t(lktsk, release_sent);
      auto rlt = t(lktsk, release_local);
      if (t_exist(rst, release_sent)) {
        timer_print(lktsk, rt->second, rst->second, "r");
      } else if (t_exist(rlt, release_local)) {
        timer_print(lktsk, rt->second, rlt->second, "rl");
      } else assert(0);
    }
  }

  for (auto const &i : schedule_end_time[0]) {

    // schedule begin -> transfer packet sent
    auto lktsk = i.first;
    auto lktsk_from = schedule_pair[0][lktsk];
    auto sst = t(lktsk_from, schedule_start);
    assert(t_exist(sst, schedule_start));
    timer_print_from_to(lktsk, lktsk_from, sst->second, i.second, "s");
  }

  for (auto const &i : txn_begin_time[0]) {
    auto txn_id = i.first;
    auto tbt = i.second;
    auto txn_type_iter = txn_type_map[0].find(txn_id);
    assert(txn_type_iter != txn_type_map[0].end());

    auto tet = t(txn_id, txn_end);
    if (!t_exist(tet, txn_end)) continue;

    uint64_t extra_time = 0;
#ifdef LOCK_LATENCY_PRECISE
    for (auto& lock_id : txn_lock_map[txn_id]) {
      auto lktsk = LKTSK(lock_id, txn_id);
      auto gtxt = t(lktsk, grant_tx);
      if (!t_exist(gtxt, grant_tx)) continue;
      auto gwt = t(lktsk, grant_w_agent);
      auto gwot = t(lktsk, grant_wo_agent);
      auto glt = t(lktsk, grant_local);
      if (t_exist(gwt, grant_w_agent)) {
        extra_time += latency(gwt->second, gtxt->second);
      } else if (t_exist(gwot, grant_wo_agent)) {
        extra_time += latency(gwot->second, gtxt->second);
      } else if (t_exist(glt, grant_local)) {
        extra_time += latency(glt->second, gtxt->second);
      } 
    }
    printf("%u,%u,%ld,et\n",txn_id, txn_type_iter->second, extra_time);
#endif

    printf("%u,%u,%ld,t\n", 
      txn_id, txn_type_iter->second, latency(tbt, tet->second) - extra_time);
  }
}

/* Record the `from` task and `to` task of lock scheduling. */
void timer_schedule_from_to(lock_id lk, task_id from, task_id to) {
  schedule_end_time[net_lcore_id()][LKTSK(lk, to)] = steady_clock::now();
  schedule_pair[net_lcore_id()][LKTSK(lk, to)] = LKTSK(lk, from);
}

/******************************************************
 * Counter utility: recording event count             *
 ******************************************************/

#ifdef FISSLOCK_COUNTERS

/**
 * Define a counter with specified name. 
 * 
 * Exposes the timer interface:
 * - count_xxx() increments the counter by 1.
 * - count_xxx_or_abort(cnt) is similar to count_xxx(), but aborts
 *   the program if the counter exceeds `cnt`.
 */
#define counter_define(name) \
static uint32_t name##_count[DPDK_LCORE_NUM];\
void count_##name() { name##_count[net_lcore_id()]++; } \
void multi_count_##name(int count) { name##_count[net_lcore_id()] += count; } \
void count_##name##_or_abort(uint64_t cnt) {\
  if (name##_count[net_lcore_id()] >= cnt) {\
    fprintf(stderr, "blocked due to too many fwd backs\n"); \
    sleep(100);\
  } \
  name##_count[net_lcore_id()]++; \
}

/* Print the counter's value to stderr. */
#define counter_report(name) do { \
  uint32_t _ct = 0; \
  for (int _c = 0; _c < DPDK_LCORE_NUM; _c++) { \
    fprintf(stderr, "[host %d] core %d %s count: %d\n", \
      LOCALHOST_ID, _c, #name, name##_count[_c]); \
    _ct += name##_count[_c]; \
  } \
  fprintf(stderr, "[host %d] %s total count: %d\n", LOCALHOST_ID, #name, _ct); \
} while (0);

#else

#define counter_define(name) \
static uint32_t name##_count[DPDK_LCORE_NUM];\
void count_##name() {} \
void count_##name##_or_abort(uint64_t cnt) {} \

#define counter_report(name) 

#endif

counter_define(fwd_back);
counter_define(grant);
counter_define(grant_with_agent);
counter_define(grant_wo_agent);
counter_define(grant_local);
counter_define(rx);
counter_define(tx);
counter_define(client_acquire);
counter_define(client_acquire_local);
counter_define(client_acquire_remote);
counter_define(client_release);
counter_define(client_release_local);
counter_define(client_release_remote);
counter_define(client_abort);
counter_define(server_acquire);
counter_define(server_grant);
counter_define(server_release);
counter_define(switch_direct_grant);
counter_define(primary);
counter_define(secondary);
counter_define(primary_acquire);
counter_define(primary_release);
counter_define(primary_grant);
counter_define(secondary_acquire);
counter_define(secondary_push_back);
counter_define(secondary_release);

void report_counters() {
  counter_report(fwd_back);
  counter_report(grant);
  counter_report(grant_with_agent);
  counter_report(grant_wo_agent);
  counter_report(grant_local);
  counter_report(rx);
  counter_report(tx);
  counter_report(client_acquire);
  counter_report(client_acquire_local);
  counter_report(client_acquire_remote);
  counter_report(client_release);
  counter_report(client_release_local);
  counter_report(client_release_remote);
  counter_report(client_abort);
  counter_report(server_acquire);
  counter_report(server_grant);
  counter_report(server_release);
  counter_report(switch_direct_grant);
  counter_report(primary);
  counter_report(secondary);
  counter_report(primary_acquire);
  counter_report(primary_release);
  counter_report(primary_grant);
  counter_report(secondary_acquire);
  counter_report(secondary_push_back);
  counter_report(secondary_release);
}

uint32_t get_tx_count_lcore(uint32_t lcore_id) {
  return tx_count[lcore_id];
}

/******************************************************
 * Profiler utility: recording event duration.        *
 ******************************************************/

/* Profilers are single-cored. */
#define profiler_define(name) \
vector<uint64_t> name##_duration; \
steady_clock::time_point name##_timestamp; \
void profile_##name##_start() { \
  name##_timestamp = steady_clock::now(); \
} \
uint64_t profile_##name##_end() { \
  auto _lat = latency(name##_timestamp, steady_clock::now()); \
  name##_duration.push_back(_lat); \
  return _lat; \
}

#define profile_print(name) do { \
  double _sum = 0; \
  for (auto duration : name##_duration) { \
    /* fprintf(stderr, "[profile-cdf] %s %ld\n", #name, duration); */ \
    _sum += duration; \
  } \
  fprintf(stderr, "[profile] %s: %.2fus\n", #name, \
    _sum / 1000 / name##_duration.size()); \
} while (0)

profiler_define(lock_lkey);
profiler_define(lock_acquire);
profiler_define(lock_mprotect);
profiler_define(unlock_release);
profiler_define(unlock_mprotect);

profiler_define(mprotect);

void report_profiler() {
  profile_print(lock_lkey);
  profile_print(lock_acquire);
  profile_print(lock_mprotect);
  profile_print(unlock_release);
  profile_print(unlock_mprotect);
}

/******************************************************
 * A more accurate version of Linux usleep().         *
 ******************************************************/

void delay(uint64_t nanosec) {
  const auto timeout = std::chrono::nanoseconds(nanosec);
  auto start = std::chrono::steady_clock::now();
  while(1) {
    auto elapsed = std::chrono::steady_clock::now() - start;
    if (elapsed >= timeout) {
      break;
    }
  }
}

/******************************************************
 * Record lock queue size of lock manager.        *
 ******************************************************/
static int cur_lock_queue_size[DPDK_LCORE_NUM];
static vector<int> lock_queue_size_array[DPDK_LCORE_NUM];

void inc_lock_queue_size() {
  int size = ++cur_lock_queue_size[net_lcore_id()];
  lock_queue_size_array[net_lcore_id()].push_back(size);
}

void dec_lock_queue_size() {
  --cur_lock_queue_size[net_lcore_id()];
}

void report_lock_queue_size() {
  for (int i = 0; i < DPDK_LCORE_NUM; i++) {
    int max_queue_size = 0;
    for (auto& queue_size : lock_queue_size_array[i]) {
      max_queue_size = max(max_queue_size, queue_size);
    }
    fprintf(stderr, "[host%u core%d] max lock queue size: %d\n", LOCALHOST_ID, i, max_queue_size);
  }
}