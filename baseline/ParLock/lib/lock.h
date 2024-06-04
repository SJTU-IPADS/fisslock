#ifndef __PARLOCK_LOCK_H
#define __PARLOCK_LOCK_H
#include <stdint.h>
#include "net.h"
#include "types.h"

/* The lock state definition.
    These are not only used to represent lock states,
    but also lock operators.
 */
typedef enum {
    FREE = 0,
    RELEASE,     /* Release lock */
    LOCK_SHARED, /* Shared acquire */
    LOCK_EXCL,   /* Exclusive acquire */
} lock_state;

#ifdef __cplusplus
extern "C" {
#endif

int lock_setup(uint32_t flags);

int process_lock_req(lock_post_header* buf, uint32_t lcore_id);
int process_unlock_req(lock_post_header* buf, uint32_t lcore_id);
int process_lock_reply(lock_post_header* buf, uint32_t lcore_id);

lock_req lock_acquire_async(uint32_t lkey, uint32_t requester, lock_state op);
int lock_release(uint32_t lkey, uint32_t requester, lock_state op);
int lock_release_after_exec(int exec_time, int core, lktsk* reqs);

bool lock_req_granted(lock_req req, uint32_t lock_id, uint32_t requester);

int lock_local_init(lock_key lock_id);

#ifdef __cplusplus
}
#endif


#endif