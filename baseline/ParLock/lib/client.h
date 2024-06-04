#ifndef __PARLOCK_CLIENT_H
#define __PARLOCK_CLIENT_H

#include "lock_queue.h"
#include "coroutine_wait_queue.h"
#include "lock.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void acquire_lock_local(uint32_t lkey, uint32_t requester, lock_state op);
void acquire_lock_remote(uint32_t lkey, uint32_t requester, lock_state op);
void release_lock_local(uint32_t lkey, uint32_t requester, lock_state op);
void release_lock_remote(uint32_t lkey, uint32_t requester, lock_state op);

#ifdef __cplusplus
}
#endif

#endif
