
#ifndef __FISSLOCK_POST_H
#define __FISSLOCK_POST_H

#include "types.h"

/* All possible types of posts.
 *
 * Each type of post contains a request and a reply.
 * Application threads sends requests and receives replies,
 * while memory & lock server threads receives requests and
 * send replies.
 */

typedef enum {
  POST_LOCK_ACQUIRE = 0x1,
  POST_LOCK_GRANT_WITH_AGENT,
  POST_LOCK_GRANT_WO_AGENT,
  POST_LOCK_RELEASE,
  POST_LOCK_TRANSFER,
  POST_LOCK_FREE,

  POST_MEM_DIFF,
  POST_CLEAR_SEQ,

  NUM_POST_TYPES,
} post_t;

/* Macros to ease post payload manipulation.
 */

#define PAYLOAD(post) ((post##_payload *)payload)

/* Post format specifications.
 */

/* The global post header */
typedef struct __attribute__((packed)) {
  unsigned short type: 8;
} post_header;

/* Lock packet format */
typedef struct __attribute__((packed)) {
  unsigned char mode_old_mode: 8;
  unsigned int lock_id: 32;
  unsigned char machine_id: 8;
  unsigned int task_id: 32;
  unsigned char agent: 8;
  unsigned int wq_size: 32;
  unsigned char ncnt: 8;
} lock_post_header;

/* Post fabricating or parsing utilities.
 */

#define POST_TYPE(buf) ((post_header *)buf)->type
#define POST_BODY(buf) (void *)((uint64_t)buf + sizeof(post_header))
#define POST_HDR_SIZE(type) (sizeof(post_header) + sizeof(type))

#define MODE_OLD_MODE(mode, old_mode) (((old_mode & 0x1) << 1) | (mode & 0x1))
#define MODE(mode_old_mode) (lock_state)((mode_old_mode & 0x1) | 0x2)
#define OLD_MODE(mode_old_mode) (lock_state)(((mode_old_mode >> 1) & 0x1) | 0x2)

#define GRANTED(hdr) ((hdr->mode_old_mode >> 6) & 0x1)
#define TRANSFERRED(hdr) ((hdr->mode_old_mode >> 5) & 0x1)
#define GET_LID_IN_HDR(hdr) (hdr->lock_id)
#define GET_MACHINE_ID_IN_HDR(hdr) (hdr->machine_id)
#define GET_TASK_ID_IN_HDR(hdr) (hdr->task_id)
#define GET_NCNT_IN_HDR(hdr) (hdr->ncnt)
#define SKIP_LOCK_HDR(hdr) (void *)((uint64_t)(hdr + 1) + hdr->wq_size * sizeof(wqe))

#define MID_TO_MGRP(mid) (mid + 192)
#define MGRP_TO_MID(mgrp) (mgrp - 192)
#define BROADCAST_MID 255

/* Packet modifier hooks.
 *
 * Any module that wants to apply modifications on packets to
 * send could register a modifier function. The modifier takes
 * the packet type and the end of the packet buffer as arguments,
 * and should return the size of added packet content.
 */
typedef size_t (*modifier_f)(post_t type, lock_post_header* hdr);

/* APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

void register_packet_modifier(modifier_f f);

int send_post_acquire(char op, lock_id lid, host_id hid, task_id tid);
int send_post_acquire_lm(char op, lock_id lid, host_id hid, task_id tid);
int send_post_transfer(char next_op, lock_id lid, host_id hid, task_id tid,
                size_t wq_size, void* wq_addr, char ncnt, char old_state);
int send_post_free(lock_id lid, char ncnt, char old_state);
int send_post_release(lock_id lid, host_id hid, task_id tid);
int send_post_release_lm(lock_id lid, host_id hid, task_id tid, host_id lm);
int send_post_grant(lock_id lid, host_id hid, task_id tid);
int send_post_clear(lock_id lid);

#ifdef __cplusplus
}
#endif

#endif