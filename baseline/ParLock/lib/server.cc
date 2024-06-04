#include <vector>
#include <rte_malloc.h>
#include "server.h"
#include "client.h"
#include "lock_queue.h"
#include "statistics.h"
#include "coroutine_wait_queue.h"
#include "statistics.h"
#include "conf.h"
#include "debug.h"

void server_handle_exclusive_lock(uint32_t lcore_id, lock_post_header* lk_hd) {
    // DEBUG("lcore %u received exclusive lock %u from requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
#ifdef PARLOCK_READER_FIRST
    uint32_t lock_id = lk_hd->lock_id;
    block_lock_queue(lock_id);
    if (get_lock_state(lock_id) == STATE_FREE) {
        // grant exclusive lock
        set_lock_state(lock_id, STATE_EXCLUSIVE);
#ifdef PARLOCK_ENQUEUE_HOLDERS
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
#endif
        unblock_lock_queue(lock_id);
        net_send(lk_hd->machine_id, POST_LOCK_REPLY, lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        timer_handle_acquire_end(lk_hd->lock_id, lk_hd->lh_id);
        count_server_grant(); 
        // DEBUG("lcore %u granted exclusive lock %u to requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
    } else {
        // enqueue the exclusive request
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        // DEBUG("lcore %u enqueued exclusive lock %u from requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
        unblock_lock_queue(lock_id);
    }
#else
    uint32_t lock_id = lk_hd->lock_id;
    block_lock_queue(lock_id);
    if (get_shared_counter(lock_id) == 0 && get_exclusive_counter(lock_id) == 0) {
        // grant the exclusive request
        add_exclusive_counter(lock_id);
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        unblock_lock_queue(lock_id);
        net_send(lk_hd->machine_id, POST_LOCK_REPLY, lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        timer_handle_acquire_end(lk_hd->lock_id, lk_hd->lh_id);
        // DEBUG("server granted exclusive lock %u to requester %u", lock_id, lk_hd->lh_id);
        // DEBUG("granted exclusive lock %u to requester %u", lock_id, lk_hd->lh_id);
        count_server_grant();
    } else {
        // enqueue the exclusive request
        add_exclusive_counter(lock_id);
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        // DEBUG("enqueued exclusive lock %u request of requester %u", lock_id, lk_hd->lh_id);
        unblock_lock_queue(lock_id);
    }
#endif
}

void server_handle_shared_lock(uint32_t lcore_id, lock_post_header* lk_hd) {
    // DEBUG("lcore %u received shared lock %u from requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
#ifdef PARLOCK_READER_FIRST
    uint32_t lock_id = lk_hd->lock_id;
    block_lock_queue(lock_id);
    if (get_lock_state(lock_id) != STATE_EXCLUSIVE) {
        // grant the shared request
        set_lock_state(lock_id, STATE_SHARED);
#ifdef PARLOCK_ENQUEUE_HOLDERS
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
#endif
        add_shared_counter(lock_id);
        unblock_lock_queue(lock_id);
        net_send(lk_hd->machine_id, POST_LOCK_REPLY, lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        timer_handle_acquire_end(lk_hd->lock_id, lk_hd->lh_id);
        count_server_grant();
        // DEBUG("lcore %u granted shared lock %u to requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
    } else {
        // enqueue the shared request
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        // DEBUG("lcore %u enqueued shared lock %u from requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
        unblock_lock_queue(lock_id);
    }
#else
    uint32_t lock_id = lk_hd->lock_id;
    block_lock_queue(lock_id);
    add_shared_counter(lock_id);
    if (get_exclusive_counter(lock_id) == 0) {
        // grant the shared request
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        unblock_lock_queue(lock_id);
        net_send(lk_hd->machine_id, POST_LOCK_REPLY, lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        timer_handle_acquire_end(lk_hd->lock_id, lk_hd->lh_id);
        // DEBUG("server granted shared lock %u to requester %u", lock_id, lk_hd->lh_id);
        count_server_grant();
    } else {
        // enqueue the shared request
        enqueue_request(lk_hd->op, lk_hd->lock_id, lk_hd->machine_id, lk_hd->lh_id);
        // DEBUG("enqueued shared lock %u request of requester %u", lock_id, lk_hd->lh_id);
        unblock_lock_queue(lock_id);
    }
#endif
}

void server_handle_exclusive_unlock(uint32_t lcore_id, lock_post_header* lk_hd) {
    // DEBUG("lcore %u received exclusive release lock %u from requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
#ifdef PARLOCK_READER_FIRST
    uint32_t lock_id = lk_hd->lock_id;
    block_lock_queue(lock_id);
    int ret = erase_lock_request(lk_hd->lock_id, lk_hd->lh_id, lk_hd->machine_id, lk_hd->op);
    if (get_lock_state(lock_id) == STATE_EXCLUSIVE && !ret) {
        // holder release lock
        set_lock_state(lock_id, STATE_FREE);
        lock_queue_entry *cur_head = get_head_request(lock_id);
        if (cur_head != NULL && cur_head->op == LOCK_EXCL) {
            // DEBUG("lcore %u switched lock %u to exclusive state", rte_lcore_id(), lk_hd->lock_id);
            // grant to next exclusive lock waiter
            set_lock_state(lock_id, STATE_EXCLUSIVE);
#ifndef PARLOCK_ENQUEUE_HOLDERS
            dequeue_request(lock_id);
#endif
            if (cur_head->host == LOCALHOST_ID) {
                // this is a local waiter
                coroutine_wait_queue_signal(lock_id, cur_head->requester);
                timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, cur_head->requester);
                timer_grant_begin(lk_hd->lock_id, cur_head->requester);
                timer_grant_local(lk_hd->lock_id, cur_head->requester);
            } else {
                // this is a remote waiter
                net_send(cur_head->host, POST_LOCK_REPLY, cur_head->op, lock_id, cur_head->host, cur_head->requester);
                timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, cur_head->requester);
                count_server_grant();
            } 
#ifndef PARLOCK_ENQUEUE_HOLDERS
            free(cur_head);
#endif
            unblock_lock_queue(lock_id);
        } else if (cur_head != NULL && cur_head->op == LOCK_SHARED) {
            // DEBUG("lcore %u switched lock %u to shared state", rte_lcore_id(), lk_hd->lock_id);
            // grant to the existing shared lock waiters
            set_lock_state(lock_id, STATE_SHARED);
#ifdef PARLOCK_ENQUEUE_HOLDERS
            std::vector<lock_queue_entry*> shared_head = get_shared_requests(lock_id);
#else
            std::vector<lock_queue_entry*> shared_head = dequeue_shared_requests(lock_id);
#endif
            // DEBUG("lcore %u got %lu shared waiters", rte_lcore_id(), shared_head.size());
            for (auto& iter : shared_head) {
                add_shared_counter(lock_id);
                if (iter->host == LOCALHOST_ID) {
                    // this is a local waiter
                    coroutine_wait_queue_signal(lock_id, iter->requester);
                    timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, iter->requester);
                    timer_grant_begin(lk_hd->lock_id, iter->requester);
                    timer_grant_local(lk_hd->lock_id, iter->requester);
                } else {
                    // this is a remote waiter
                    net_send(iter->host, POST_LOCK_REPLY, iter->op, lock_id, iter->host, iter->requester);
                    timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, iter->requester);
                    count_server_grant();
                }
#ifndef PARLOCK_ENQUEUE_HOLDERS
                free(iter);
#endif
            }
            unblock_lock_queue(lock_id);
        } else {
            // empty queue
            unblock_lock_queue(lock_id);
        }
    } else {
        // abort release
        unblock_lock_queue(lock_id);
    }
#else
    uint32_t lock_id = lk_hd->lock_id;
    // DEBUG("received exclusive release lock %u from requester %u", lock_id, lk_hd->lh_id);
    block_lock_queue(lock_id);
    lock_queue_entry *cur_head = get_head_request(lock_id);
    if (cur_head->requester != lk_hd->lh_id) {
        // If not lock holder send release, just remove it from lock queue
        int ret = erase_lock_request(lk_hd->lock_id, lk_hd->lh_id, lk_hd->machine_id, lk_hd->op);
        if (ret == 1) {
            sub_exclusive_counter(lock_id);
        }
        unblock_lock_queue(lock_id);
        return;
    } else {
        free(dequeue_request(lock_id));
        sub_exclusive_counter(lock_id);
    }
    // Don't free this head, because it is still in the queue
    lock_queue_entry *head = get_head_request(lock_id);
    if (head != NULL && head->op == LOCK_EXCL) {
        uint32_t tid = head->requester;
        unblock_lock_queue(lock_id);
        if (head->host == LOCALHOST_ID) {
            // this is a local waiter
            coroutine_wait_queue_signal(lock_id, tid);
            timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, tid);
            timer_grant_begin(lk_hd->lock_id, tid);
            timer_grant_local(lk_hd->lock_id, tid);
        } else {
            // this is a remote waiter
            net_send(head->host, POST_LOCK_REPLY, head->op, lock_id, head->host, tid);
            timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, tid);
            count_server_grant();
            // DEBUG("server granted next waiter %u of lock %u", head->requester, lock_id);
        }
    } else if (head != NULL && head->op == LOCK_SHARED) {
        auto shared_head_ptrs = get_head_shared_requests(lock_id);
        std::vector<lock_queue_entry> shared_head;
        for (auto iter : shared_head_ptrs) shared_head.push_back(*iter);
        unblock_lock_queue(lock_id);

        for (auto iter : shared_head) {
            if (iter.host == LOCALHOST_ID) {
                // this is a local waiter
                coroutine_wait_queue_signal(lock_id, iter.requester);
                timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, iter.requester);
                timer_grant_begin(lk_hd->lock_id, iter.requester);
                timer_grant_local(lk_hd->lock_id, iter.requester);
            } else {
                // this is a remote waiter
                // DEBUG("grant lock %u to shared waiter %u", lock_id, iter->requester);
                net_send(iter.host, POST_LOCK_REPLY, iter.op, lock_id, iter.host, iter.requester);
                timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, iter.requester);
                count_server_grant();
            }
        }
    } else {
        unblock_lock_queue(lock_id);
    }
#endif
}

void server_handle_shared_unlock(uint32_t lcore_id, lock_post_header* lk_hd) {
    // DEBUG("lcore %u received shared release lock %u from requester %u", rte_lcore_id(), lk_hd->lock_id, lk_hd->lh_id);
#ifdef PARLOCK_READER_FIRST
    uint32_t lock_id = lk_hd->lock_id;
    block_lock_queue(lock_id);
    int ret = erase_lock_request(lk_hd->lock_id, lk_hd->lh_id, lk_hd->machine_id, lk_hd->op);
    if (get_lock_state(lock_id) == STATE_SHARED && !ret) {
        // holder release lock
        sub_shared_counter(lock_id);
        if (get_shared_counter(lock_id) > 0) {
            unblock_lock_queue(lock_id);
            return;
        }
        set_lock_state(lock_id, STATE_FREE);
        lock_queue_entry *cur_head = get_head_request(lock_id);
        if (cur_head != NULL && cur_head->op == LOCK_EXCL) {
            // DEBUG("lcore %u switched lock %u to exclusive state", rte_lcore_id(), lk_hd->lock_id);
            // grant to next exclusive lock waiter
            set_lock_state(lock_id, STATE_EXCLUSIVE);
#ifndef PARLOCK_ENQUEUE_HOLDERS
            dequeue_request(lock_id);
#endif
            if (cur_head->host == LOCALHOST_ID) {
                // this is a local waiter
                coroutine_wait_queue_signal(lock_id, cur_head->requester);
                timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, cur_head->requester);
                timer_grant_begin(lk_hd->lock_id, cur_head->requester);
                timer_grant_local(lk_hd->lock_id, cur_head->requester);
            } else {
                // this is a remote waiter
                net_send(cur_head->host, POST_LOCK_REPLY, cur_head->op, lock_id, cur_head->host, cur_head->requester);
                timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, cur_head->requester);
                count_server_grant();
            } 
#ifndef PARLOCK_ENQUEUE_HOLDERS
            free(cur_head);
#endif
            unblock_lock_queue(lock_id);
        } else if (cur_head != NULL && cur_head->op == LOCK_SHARED) {
            // abnormal condition which should not appear
            ERROR("lcore %u Abnormal condition in shared unlock: shared waiter %u in the queue when state is shared", rte_lcore_id(), cur_head->requester);
            unblock_lock_queue(lock_id);
        } else {
            unblock_lock_queue(lock_id);
        }
    } else if (get_lock_state(lock_id) == STATE_SHARED) {
        // abort release when lock state is shared
        unblock_lock_queue(lock_id);
        ERROR("lcore %u Abnormal condition in shared unlock %u when lock state is shared: unlocker %u abort release", rte_lcore_id(), lock_id, lk_hd->lh_id);
    } else {
        // abort release when lock state is exclusive
        unblock_lock_queue(lock_id);
    }
#else
    uint32_t lock_id = lk_hd->lock_id;
    block_lock_queue(lock_id);
    lock_queue_entry *cur_head = get_head_request(lock_id);
    if (cur_head->requester != lk_hd->lh_id) {
        // If not lock holder send release, just remove it from lock queue
        int ret = erase_lock_request(lk_hd->lock_id, lk_hd->lh_id, lk_hd->machine_id, lk_hd->op);
        if (ret == 1) {
            sub_shared_counter(lock_id);
        }
        unblock_lock_queue(lock_id);
        return;
    } else {
        free(dequeue_request(lock_id));
        sub_shared_counter(lock_id);
    }
    // Don't free this head, because it is still in the queue
    lock_queue_entry *head = get_head_request(lock_id);
    if (head != NULL && head->op == LOCK_EXCL) {
        uint32_t tid = head->requester;
        unblock_lock_queue(lock_id);
        // if the waiter if exclusive, grant the lock
        if (head->host == LOCALHOST_ID) {
            // this is a local waiter
            coroutine_wait_queue_signal(lock_id, tid);
            timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, tid);
            timer_grant_begin(lk_hd->lock_id, tid);
            timer_grant_local(lk_hd->lock_id, tid);
        } else {
            // this is a remote waiter
            net_send(head->host, POST_LOCK_REPLY, head->op, lock_id, head->host, tid);
            timer_schedule_from_to(lk_hd->lock_id, lk_hd->lh_id, tid);
            count_server_grant();
        }
    } else unblock_lock_queue(lock_id);
#endif
}
