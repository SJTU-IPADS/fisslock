#include "lock.h"
#include "server.h"
#include "statistics.h"
#include "coroutine_wait_queue.h"
#include "lock_queue.h"
#include "net.h"
#include "client.h"

int process_lock_req(lock_post_header* lk_hd, uint32_t lcore_id) {
    timer_since_burst_handle_acquire_begin(lk_hd->lock_id, lk_hd->lh_id);
    count_server_acquire();
    if (lk_hd->op == LOCK_EXCL) {
        server_handle_exclusive_lock(lcore_id, lk_hd);
    } else if (lk_hd->op == LOCK_SHARED) {
        server_handle_shared_lock(lcore_id, lk_hd);
    }
    return 0;
}

int process_unlock_req(lock_post_header* lk_hd, uint32_t lcore_id) {
    timer_since_burst_schedule_start(lk_hd->lock_id, lk_hd->lh_id);
    count_server_release();
    if (lk_hd->op == LOCK_EXCL) {
        server_handle_exclusive_unlock(lcore_id, lk_hd);
    } else if (lk_hd->op == LOCK_SHARED) {
        server_handle_shared_unlock(lcore_id, lk_hd);
    }
    return 0;
}

int process_lock_reply(lock_post_header* lk_hd, uint32_t lcore_id) {
    timer_grant_begin(lk_hd->lock_id, lk_hd->lh_id);
    coroutine_wait_queue_signal(lk_hd->lock_id, lk_hd->lh_id);
    timer_grant_wo_agent(lk_hd->lock_id, lk_hd->lh_id);
    return 0;
}

int lock_setup(uint32_t flags) {
    register_packet_dispatcher(POST_LOCK_REQ, process_lock_req);
    register_packet_dispatcher(POST_LOCK_REPLY, process_lock_reply);
    register_packet_dispatcher(POST_UNLOCK_REQ, process_unlock_req);
    return 0;
}

lock_req lock_acquire_async(uint32_t lkey, uint32_t requester, lock_state op) {
    timer_acquire(lkey, requester);
    uint32_t lcore_id = rte_lcore_id();
    add_to_wait_queue(lkey, requester);
    count_client_acquire();
    uint8_t host_id = get_lock_server(lkey);
    if (host_id == LOCALHOST_ID) {
        count_client_acquire_local();
        acquire_lock_local(lkey, requester, op);
    } else {
        count_client_acquire_remote();
        acquire_lock_remote(lkey, requester, op);
        timer_acquire_sent(lkey, requester);
    }
    return static_cast<lock_req>((static_cast<lock_req>(lkey) << 32) | requester);
}

int lock_release(uint32_t lkey, uint32_t requester, lock_state op) {
    timer_release(lkey, requester);
    uint32_t lcore_id = rte_lcore_id();
    count_client_release();
    uint8_t host_id = get_lock_server(lkey);
    if (host_id == LOCALHOST_ID) {
        count_client_release_local();
        timer_release_local(lkey, requester);
        release_lock_local(lkey, requester, op);
    } else {
        count_client_release_remote();
        release_lock_remote(lkey, requester, op);
        timer_release_sent(lkey, requester);
    }
    return 0;
}

int lock_release_after_exec(int exec_time, int core, lktsk* reqs) {
    return 0;
}

bool lock_req_granted(lock_req req, uint32_t lid, uint32_t tid) {
    uint32_t lock_id = ((req >> 32) & (uint32_t)(0xffffffff));
    uint32_t requester = (req & (uint32_t)(0xffffffff));
    return coroutine_wait_queue_wait(lock_id, requester);
}

int lock_local_init(lock_key lock_id) {
    init_lock(lock_id);
    init_lock_wait_table(lock_id);
    return 0;
}
