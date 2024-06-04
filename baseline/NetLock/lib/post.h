#ifndef __NETLOCK_POST_H
#define __NETLOCK_POST_H

#include <stdint.h>
#include "types.h"

typedef enum {
  POST_LOCK_ACQUIRE = 0x1,
  POST_LOCK_GRANT_WITH_AGENT,
  POST_LOCK_GRANT_WO_AGENT,
  POST_LOCK_RELEASE,
  POST_LOCK_TRANSFER,
  POST_LOCK_FREE,

  POST_MEM_DIFF,

  NUM_POST_TYPES,
} post_t;

typedef struct __attribute__((packed)) {
  uint8_t recirc_flag;
  uint8_t op_type;
  uint8_t mode;
  uint8_t client_id;
  uint32_t txn_id;
  uint32_t lock_id;
  uint64_t timestamp;
  uint32_t empty_slots;
  uint32_t head;
  uint32_t tail;
  uint8_t ncnt;
  uint8_t transferred;
} lock_post_header;

#define POST_BODY(buf) (void*)(buf)
#define TRANSFERRED(hdr) ((hdr->transferred) & 0x1)
#define GET_LID_IN_HDR(hdr) (hdr->lock_id)
#define GET_MACHINE_ID_IN_HDR(hdr) (hdr->client_id)
#define GET_TASK_ID_IN_HDR(hdr) (hdr->txn_id)
#define GET_NCNT_IN_HDR(hdr) (hdr->ncnt)
#define SKIP_LOCK_HDR(hdr) (void *)((uint64_t)(hdr + 1))

int send_post_clear(lock_id lid);

#endif