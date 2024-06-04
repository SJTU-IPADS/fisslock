#ifndef __NETLOCK_LOCK_H
#define __NETLOCK_LOCK_H

#include <stdint.h>

#include "types.h"
#include "post.h"
#include "coroutine_wait_queue.h"

#define LK_PORT 8888
#define MEM_DIFF_PORT 18888
#define ACQUIRE_LOCK 0
#define RELEASE_LOCK 1
#define PUSH_BACK_LOCK 2
#define GRANT_LOCK_FROM_SERVER 3
#define DIRECT_GRANT_FROM_SWITCH 4
#define MEM_DIFF 5
#define PRIMARY_BACKUP 1
#define SECONDARY_BACKUP 2
#define SHARED 0
#define EXCLUSIVE 1

typedef enum {
    FREE = 0,
    RELEASE,     /* Release lock */
    LOCK_SHARED, /* Shared acquire */
    LOCK_EXCL,   /* Exclusive acquire */
} lock_state;

typedef struct {
  uint8_t op_type;
  uint8_t mode;
  uint8_t client_id;
  uint32_t txn_id;
  uint32_t lock_id;
  uint32_t ip_src;
  uint8_t role;
  void* payload;
  size_t payload_size;
} lock_queue_node;

#define STATE_FREE 0
#define STATE_SHARED 1
#define STATE_EXCLUSIVE 2

typedef void (*preprocessor_f)(post_t type, lock_post_header* buf);

#ifdef __cplusplus
extern "C" {
#endif

/* Applications are allowed to specify a packet pre-processor function,
 * which is executed before the lock packet is processed.
 * The `buf` argument specifies the start address of the lock packet
 * payload, i.e., after the wait queue.
 */
void register_packet_preprocessor(preprocessor_f f);

int lock_setup(uint32_t flags);
lock_req lock_acquire_async(uint32_t lkey, uint32_t requester, lock_state op);
int lock_release(uint32_t lkey, uint32_t requester, lock_state op);
int lock_release_after_exec(int exec_time, int core, lktsk* reqs);

int lock_local_init(lock_key lock_id);

#ifdef __cplusplus
}
#endif


#endif