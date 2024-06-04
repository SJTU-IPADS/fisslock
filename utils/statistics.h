
#ifndef __FISSLOCK_STATISTICS_H
#define __FISSLOCK_STATISTICS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void delay(uint64_t nanosec);

/**
 * Throughput
*/
void add_txn_lock_map(uint32_t txn_id, uint32_t lock_id);
void txn_thpt_record(uint32_t lcore_id, uint32_t c_id, 
  uint64_t txn_cnt, uint64_t request_cnt, int64_t nano_duration);
void dynamic_thpt_record(uint32_t core, int tick, double thpt);
void report_thpt();

/**
 * Timer
*/
#define timer_declare(name) \
void timer_##name(lock_id lock, task_id task);\
void timer_since_burst_##name(lock_id lock, task_id task)

timer_declare(acquire);
timer_declare(acquire_sent);
timer_declare(grant_begin);
timer_declare(grant_w_agent);
timer_declare(grant_wo_agent);
timer_declare(grant_local);
timer_declare(grant_tx);
timer_declare(release);
timer_declare(release_sent);
timer_declare(release_local);
timer_declare(schedule_start);
timer_declare(schedule_end);
timer_declare(handle_acquire_begin);
timer_declare(handle_acquire_end);
timer_declare(queue_start);
timer_declare(queue_end);
timer_declare(secondary_begin);
timer_declare(secondary_end);
timer_declare(switch_direct_grant);

void set_burst_time();
void timer_schedule_from_to(lock_id lk, task_id from, task_id to);

void timer_start();
uint64_t timer_now();

void timer_txn_begin(uint32_t txn_id, uint32_t txn_type);
void timer_txn_end(uint32_t txn_id);

void report_timer();

/**
 * Counter
*/
#define counter_declare(name) \
void count_##name(); \
void multi_count_##name(int count); \
void count_##name##_or_abort(uint64_t cnt)

counter_declare(fwd_back);
counter_declare(grant);
counter_declare(grant_with_agent);
counter_declare(grant_wo_agent);
counter_declare(grant_local);
counter_declare(rx);
counter_declare(tx);
counter_declare(client_acquire);
counter_declare(client_acquire_local);
counter_declare(client_acquire_remote);
counter_declare(client_release);
counter_declare(client_release_local);
counter_declare(client_release_remote);
counter_declare(client_abort);
counter_declare(server_acquire);
counter_declare(server_grant);
counter_declare(server_release);
counter_declare(switch_direct_grant);
counter_declare(primary);
counter_declare(secondary);
counter_declare(primary_acquire);
counter_declare(primary_release);
counter_declare(primary_grant);
counter_declare(secondary_acquire);
counter_declare(secondary_push_back);
counter_declare(secondary_release);

uint32_t get_tx_count_lcore(uint32_t lcore_id);
void report_counters();

/**
 * Profiler
*/

#define profiler_declare(name) \
void profile_##name##_start(); \
uint64_t profile_##name##_end();

profiler_declare(lock_lkey);
profiler_declare(lock_acquire);
profiler_declare(lock_mprotect);
profiler_declare(unlock_release);
profiler_declare(unlock_mprotect);

profiler_declare(mprotect);

void report_profiler();

/**
 * Record queue size of lock manager
*/
void inc_lock_queue_size();
void dec_lock_queue_size();
void report_lock_queue_size();

#ifdef __cplusplus
}
#endif

#endif