
#ifndef __FISSLOCK_TYPES_H
#define __FISSLOCK_TYPES_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t host_id;
typedef uint64_t addr_t;
typedef uint32_t lock_id;
typedef uint32_t task_id;
typedef uint64_t lock_key;
typedef uint64_t lktsk;

#define LKTSK(lid, tid) (((uint64_t)lid << 32) ^ (uint64_t)tid)

/* The lock request abstraction for reducing the overhead of
 * lock granting.
 */
typedef uint64_t lock_req;

#endif