#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>

#include "lock.h"
#include "statistics.h"
#include "net.h"
#include "statistics.h"

/* Register the packet preprocessor.
 */
static preprocessor_f packet_preprocess;
void register_packet_preprocessor(preprocessor_f f) {
  packet_preprocess = f;
}

/* The dispatcher for lock packets.
 */
int lock_packet_dispatch(void* buf, uint32_t lcore_id) {
    lock_post_header* message_header = static_cast<lock_post_header*>(buf);
    uint32_t lock_id = ntohl(message_header->lock_id);
    uint32_t txn_id = ntohl(message_header->txn_id);

    // Execute the application-defined preprocessor
    if (packet_preprocess != NULL) {
        post_t post_type;
        if (message_header->op_type == DIRECT_GRANT_FROM_SWITCH) {
            post_type = POST_LOCK_GRANT_WITH_AGENT;
            packet_preprocess(post_type, message_header);
        } else if (message_header->op_type == GRANT_LOCK_FROM_SERVER) {
            post_type = POST_LOCK_GRANT_WITH_AGENT;
            packet_preprocess(post_type, message_header);
        }
    }

    timer_grant_begin(lock_id, txn_id);
    if (message_header->op_type == DIRECT_GRANT_FROM_SWITCH) {
        timer_switch_direct_grant(lock_id, txn_id);
        count_switch_direct_grant();
    } else if (message_header->op_type == GRANT_LOCK_FROM_SERVER) {
        timer_grant_wo_agent(lock_id, txn_id);
    } else {
        return 0;
    }
    coroutine_wait_queue_signal(lcore_id, lock_id, txn_id);
    return 0;
}

int lock_setup(uint32_t flags) {
    register_packet_dispatcher(POST_LOCK_GRANT_WO_AGENT, lock_packet_dispatch);
    return 0;
}

lock_req lock_acquire_async(uint32_t lkey, uint32_t requester, lock_state op) {
    uint32_t lcore_id = rte_lcore_id();
    timer_acquire(lkey, requester);
    add_to_wait_queue(lcore_id, lkey, requester);
    if (op == LOCK_EXCL) {
        net_send(lcore_id, requester, ACQUIRE_LOCK, lkey, EXCLUSIVE);
    } else if (op == LOCK_SHARED) {
        net_send(lcore_id, requester, ACQUIRE_LOCK, lkey, SHARED);
    }
    timer_acquire_sent(lkey, requester);
    count_client_acquire();
    return static_cast<lock_req>((static_cast<lock_req>(lkey) << 32) | requester);
}

int lock_release(uint32_t lkey, uint32_t requester, lock_state op) {
    uint32_t lcore_id = rte_lcore_id();
    timer_release(lkey, requester);
    if (op == LOCK_EXCL) {
        net_send(lcore_id, requester, RELEASE_LOCK, lkey, EXCLUSIVE);
    } else if (op == LOCK_SHARED) {
        net_send(lcore_id, requester, RELEASE_LOCK, lkey, SHARED);
    }
    timer_release_sent(lkey, requester);
    count_client_release();
    return 0;
}

int lock_release_after_exec(int exec_time, int core, lktsk* reqs) {
    return 0;
}

int lock_local_init(lock_key lock_id) {
    init_lock_wait_table(lock_id);
    return 0;
}