#ifndef __PARLOCK_LOCK_QUEUE_H
#define __PARLOCK_LOCK_QUEUE_H

#include <vector>
#include "lock.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t op;
    uint32_t requester;
    uint8_t host;
} lock_queue_entry;

const int STATE_FREE = 0;
const int STATE_SHARED = 1;
const int STATE_EXCLUSIVE = 2;

void enqueue_request(uint8_t op, uint32_t lock_id, uint8_t machine_id, uint32_t lh_id);
lock_queue_entry* dequeue_request(uint32_t lock_id);
lock_queue_entry* get_head_request(uint32_t lock_id);
bool is_lock_holder(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op);
std::vector<lock_queue_entry*> get_head_shared_requests(uint32_t lock_id);
std::vector<lock_queue_entry*> dequeue_shared_requests(uint32_t lock_id);
std::vector<lock_queue_entry*> get_shared_requests(uint32_t lock_id);
int get_shared_counter(uint32_t lock_id);
int get_exclusive_counter(uint32_t lock_id);
int get_lock_state(uint32_t lock_id);
void set_lock_state(uint32_t lock_id, int lock_state_);
void add_shared_counter(uint32_t lock_id);
void sub_shared_counter(uint32_t lock_id);
void add_exclusive_counter(uint32_t lock_id);
void sub_exclusive_counter(uint32_t lock_id);
void block_lock_queue(uint32_t lock_id);
void unblock_lock_queue(uint32_t lock_id);
int erase_lock_request(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op);
void init_lock(uint32_t lock_id);
void init_all_locks();

#ifdef __cplusplus
}
#endif



#endif