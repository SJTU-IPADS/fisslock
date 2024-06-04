#ifndef __PARLOCK_COROUTINE_WAIT_QUEUE_H
#define __PARLOCK_COROUTINE_WAIT_QUEUE_H

#include <unordered_map>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_lock_wait_table(uint32_t lock_id);
void add_to_wait_queue(uint32_t lock_id, uint32_t requester);
void coroutine_wait_queue_signal(uint32_t lock_id, uint32_t requester);
bool coroutine_wait_queue_wait(uint32_t lock_id, uint32_t requester);

#ifdef __cplusplus
}
#endif

#endif