#ifndef __NETLOCK_COROUTINE_WAIT_QUEUE_H
#define __NETLOCK_COROUTINE_WAIT_QUEUE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_lock_wait_table(uint32_t lock_id);
void add_to_wait_queue(uint32_t lcore_id, uint32_t lock_id, uint32_t requester);
void coroutine_wait_queue_signal(uint32_t lcore_id, uint32_t lock_id, uint32_t requester);
bool lock_req_granted(lock_req req, uint32_t lid, uint32_t tid);

#ifdef __cplusplus
}
#endif

#endif