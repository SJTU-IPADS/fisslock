
#include <arpa/inet.h>
#include <string.h>

#include "debug.h"
#include "post.h"
#include "statistics.h"
#include "lock.h"
#include "net.h"

/* Packet modifier hooks.
 */
static modifier_f packet_modify;

void register_packet_modifier(modifier_f f) {
  packet_modify = f;
}

/* The generic function for sending lock requests.
 */
static int send_post_lock_generic(char t, char op, lock_id lid, 
  host_id hid, task_id tid, size_t wq_size, void* wq_addr, char ncnt,
  lock_state old_state, host_id dest) {

  uint64_t bhdl = net_new_buf();
  char* buf = net_get_sendbuf(bhdl);
  POST_TYPE(buf) = t;

  // Fabricate the post header.
  uint32_t post_size = 0;
  lock_post_header* hdr = POST_BODY(buf);
  hdr->mode_old_mode = MODE_OLD_MODE(op, old_state);
  hdr->lock_id = htonl(lid);
  hdr->machine_id = hid;
  hdr->task_id = tid;
  hdr->agent = dest;
  hdr->wq_size = wq_size;
  hdr->ncnt = ncnt;
  post_size += POST_HDR_SIZE(lock_post_header);

  // Fill the wait queue into the post body.
  void* wq_buf = buf + POST_HDR_SIZE(lock_post_header);
  if (wq_size > 0) {
    net_memcpy(wq_buf, wq_addr, wq_size * sizeof(wqe));
    post_size += wq_size * sizeof(wqe);
  }

  // Apply the packet modifier.
  if (packet_modify != NULL)
    post_size += packet_modify(t, hdr);

  net_send(0, bhdl, post_size);
  return 0;
}

int send_post_acquire(char op, lock_id lid, host_id hid, task_id tid) {
  return send_post_lock_generic(POST_LOCK_ACQUIRE, 
    op, lid, hid, tid, 0, NULL, 0, 0, LOCALHOST_ID);
}

int send_post_acquire_lm(char op, lock_id lid, host_id hid, task_id tid) {
  return send_post_lock_generic(POST_LOCK_ACQUIRE, 
    op, lid, LOCALHOST_ID, tid, 0, NULL, 0, 0, hid);
}

int send_post_transfer(char next_op, lock_id lid, host_id hid, task_id tid,
              size_t wq_size, void* wq_addr, char ncnt, char old_state) {
  return send_post_lock_generic(POST_LOCK_TRANSFER, 
    next_op, lid, hid, tid, wq_size, wq_addr, ncnt, old_state, LOCALHOST_ID);
}

int send_post_free(lock_id lid, char ncnt, char old_state) {
  return send_post_lock_generic(POST_LOCK_FREE, 0, lid, 
    LOCALHOST_ID, 0, 0, NULL, ncnt, old_state, LOCALHOST_ID);
}

int send_post_release(lock_id lid, host_id hid, task_id tid) {
  return send_post_lock_generic(POST_LOCK_RELEASE,
    0, lid, hid, tid, 0, NULL, 0, 0, LOCALHOST_ID);
}

int send_post_release_lm(lock_id lid, host_id hid, task_id tid, host_id lm) {
  return send_post_lock_generic(POST_LOCK_RELEASE,
    0, lid, hid, tid, 0, NULL, 0, 0, lm);
}

int send_post_grant(lock_id lid, host_id hid, task_id tid) {
  return send_post_lock_generic(POST_LOCK_GRANT_WO_AGENT,
    0, lid, hid, tid, 0, NULL, 0, 0, hid);
}

int send_post_clear(lock_id lid) {
  return send_post_lock_generic(POST_CLEAR_SEQ, 0, lid, 0, 0, 0, NULL, 0, 0, 0);
}