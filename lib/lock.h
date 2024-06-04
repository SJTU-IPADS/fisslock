
#ifndef __FISSLOCK_LOCK_H
#define __FISSLOCK_LOCK_H

#include <pthread.h>
#include <sys/mman.h>

#include <conf.h>
#include <post.h>

// #define LOCK_LOG_VERBOSE

/* The host-side lock implementation.
 *
 * To ensure transparency to applications, pthread mutex
 * objects are placed in the shared memory, so that all threads
 * (on different hosts) see the same mutex at a certain address.
 * Hence, we use the mutex object address as the identifier
 * of in-network locks.
 * 
 * For efficiency, after registering the mutex address in the
 * control plane, the switch returns a lock id that should be
 * cached locally. Any subsequent lock operations should use
 * the lock id as identification.
 */

#define LOCK_KEY(lock) ((lock_key)(lock))

typedef uint64_t lock_desc;


/* The lock state definition.
 * These are not only used to represent lock states,
 * but also lock operators.
 */

typedef enum {
  FREE        = 0x0,
  RELEASE     = 0x1,
  LOCK_SHARED = 0x2,
  LOCK_EXCL   = 0x3,
} lock_state;

/* The lock holder definition.
 * Each holder consists of the server it locates (host)
 * and an intra-server ID (e.g., thread ID, transaction ID).
 */

typedef uint64_t lock_holder;

#define LOCK_HOLDER(host, task) (((uint64_t)(host) << 32) ^ (uint64_t)(task))
#define LOCK_HOLDER_TASK(lh) (task_id)(lh)
#define LOCK_HOLDER_HOST(lh) (host_id)(lh >> 32)

/* The wait queue definition.
 * Currently we only record the identity of each waiter.
 */

typedef struct __attribute__((packed)) {
  unsigned char op: 2;
  unsigned int task: 30;
  unsigned char host: 8;
} wqe;

/* Applications are allowed to specify a packet pre-processor function,
 * which is executed before the lock packet is processed.
 * The `buf` argument specifies the start address of the lock packet
 * payload, i.e., after the wait queue.
 */
typedef void (*preprocessor_f)(post_t type, lock_post_header* hdr);
void register_packet_preprocessor(preprocessor_f f);

/* Magic numbers for identifying special lock packets.
 */

#define TASK_FOR_RESTORE (task_id)(-1)

/* APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

int lock_setup(uint32_t flags);

lock_id lock_init(lock_key lkey);
lock_id lock_local_init(lock_key lock_id);

int lock_acquire(lock_key lkey, task_id task, lock_state op);
lock_req lock_acquire_async(lock_key lkey, task_id task, lock_state op);
int lock_req_granted(lock_req req, lock_id lock, task_id task);
int lock_wait_local(lock_key lkey, task_id task);
int lock_release(lock_key lkey, task_id task, lock_state op);
int lock_release_after_exec(int exec_time, int core, lktsk* reqs);

int agent_daemon(void* args);

void integrity_check(uint64_t fault_code);

#ifdef __cplusplus
}
#endif

#endif