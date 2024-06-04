#include "client.h"
#include "statistics.h"
#include "statistics.h"
#include "debug.h"
#include "coroutine_wait_queue.h"
#include "conf.h"

void acquire_lock_local(uint32_t lkey, uint32_t requester, lock_state op) {
    uint32_t lcore_id = rte_lcore_id();
    block_lock_queue(lkey);
#ifdef PARLOCK_READER_FIRST
    if (op == LOCK_EXCL) {
        // DEBUG("lcore %u requester %u exclusive acquire lock %u", rte_lcore_id(), requester, lkey);
       if (get_lock_state(lkey) == STATE_FREE) {
            // grant exclusive lock
            set_lock_state(lkey, STATE_EXCLUSIVE);
#ifdef PARLOCK_ENQUEUE_HOLDERS
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
#endif
            timer_acquire_sent(lkey, requester);
            timer_grant_begin(lkey, requester);
            unblock_lock_queue(lkey);
            coroutine_wait_queue_signal(lkey, requester);
            timer_grant_local(lkey, requester);
            // DEBUG("lcore %u requster %u exclusive lock %u granted", rte_lcore_id(), requester, lkey);
        } else {
            // enqueue the exclusive request
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
            unblock_lock_queue(lkey);
            timer_acquire_sent(lkey, requester);
            // DEBUG("lcore %u requester %u exclusive lock %u enqueued", rte_lcore_id(), requester, lkey);
        } 
    } else if (op == LOCK_SHARED) {
        // DEBUG("lcore %u requester %u shared acquire lock %u", rte_lcore_id(), requester, lkey);
        if (get_lock_state(lkey) != STATE_EXCLUSIVE) {
            // granted
            set_lock_state(lkey, STATE_SHARED);
#ifdef PARLOCK_ENQUEUE_HOLDERS
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
#endif
            timer_acquire_sent(lkey, requester);
            timer_grant_begin(lkey, requester);
            add_shared_counter(lkey);
            unblock_lock_queue(lkey);
            coroutine_wait_queue_signal(lkey, requester);
            timer_grant_local(lkey, requester);
            // DEBUG("lcore %u requster %u shared lock %u granted", rte_lcore_id(), requester, lkey);
        } else {
            // enqueue lock queue
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
            unblock_lock_queue(lkey);
            timer_acquire_sent(lkey, requester);
            // DEBUG("lcore %u requester %u shared lock %u enqueued", rte_lcore_id(), requester, lkey);
        }
    } else {
        unblock_lock_queue(lkey);
    } 
#else
    if (op == LOCK_EXCL) {
        if (get_shared_counter(lkey) == 0 && get_exclusive_counter(lkey) == 0) {
            // granted 
            timer_acquire_sent(lkey, requester);
            timer_grant_begin(lkey, requester);
            add_exclusive_counter(lkey);
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
            unblock_lock_queue(lkey);
            coroutine_wait_queue_signal(lkey, requester);
            timer_grant_local(lkey, requester);
        } else {
            // enqueue lock queue
            add_exclusive_counter(lkey);
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
            unblock_lock_queue(lkey);
            timer_acquire_sent(lkey, requester);
        }
    } else if (op == LOCK_SHARED) {
        if (get_exclusive_counter(lkey) == 0) {
            // granted
            timer_acquire_sent(lkey, requester);
            timer_grant_begin(lkey, requester);
            add_shared_counter(lkey);
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
            unblock_lock_queue(lkey);
            coroutine_wait_queue_signal(lkey, requester);
            timer_grant_local(lkey, requester);
        } else {
            // enqueue lock queue
            add_shared_counter(lkey);
            enqueue_request(op, lkey, LOCALHOST_ID, requester);
            unblock_lock_queue(lkey);
            timer_acquire_sent(lkey, requester);
        }
    } else {
        unblock_lock_queue(lkey);
    }
#endif
}

void acquire_lock_remote(uint32_t lkey, uint32_t requester, lock_state op) {
    net_send(get_lock_server(lkey), POST_LOCK_REQ, op, lkey, LOCALHOST_ID, requester);
}

void release_lock_local(uint32_t lkey, uint32_t requester, lock_state op) {
    timer_schedule_start(lkey, requester);
    uint32_t lcore_id = rte_lcore_id();
    block_lock_queue(lkey);
#ifdef PARLOCK_READER_FIRST
    int ret = erase_lock_request(lkey, requester, LOCALHOST_ID, op);
    if (op == LOCK_EXCL) {
        // DEBUG("lcore %u requester %u exclusive release lock %u", rte_lcore_id(), requester, lkey);
        if (get_lock_state(lkey) == STATE_EXCLUSIVE && !ret) {
            // holder release lock
            set_lock_state(lkey, STATE_FREE);
            lock_queue_entry *head = get_head_request(lkey);
            if (head != NULL && head->op == LOCK_EXCL) {
                // DEBUG("lcore %u switched lock %u to exclusive state", rte_lcore_id(), lkey);
                uint32_t tid = head->requester;
#ifndef PARLOCK_ENQUEUE_HOLDERS
                dequeue_request(lkey);
#endif
                set_lock_state(lkey, STATE_EXCLUSIVE);
                if (head->host == LOCALHOST_ID) {
                    // this is a local waiter
                    coroutine_wait_queue_signal(lkey, tid);
                    timer_schedule_from_to(lkey, requester, tid);
                    timer_grant_begin(lkey, tid);
                    timer_grant_local(lkey, tid);
                } else {
                    // this is a remote waiter
                    net_send(head->host, POST_LOCK_REPLY, head->op, lkey, head->host, head->requester);
                    timer_schedule_from_to(lkey, requester, head->requester);
                    count_server_grant();
                }
#ifndef PARLOCK_ENQUEUE_HOLDERS
                free(head);
#endif
                unblock_lock_queue(lkey);
            } else if (head != NULL && head->op == LOCK_SHARED) {
                // DEBUG("lcore %u switched lock %u to shared state", rte_lcore_id(), lkey);
                set_lock_state(lkey, STATE_SHARED);
#ifdef PARLOCK_ENQUEUE_HOLDERS
                std::vector<lock_queue_entry*> shared_head = get_shared_requests(lkey);
#else
                std::vector<lock_queue_entry*> shared_head = dequeue_shared_requests(lkey);
#endif
                // DEBUG("lcore %u got %lu shared waiters", rte_lcore_id(), shared_head.size());
                for (auto& iter : shared_head) {
                    add_shared_counter(lkey);
                    if (iter->host == LOCALHOST_ID) {
                        // this is a local waiter
                        coroutine_wait_queue_signal(lkey, iter->requester);
                        timer_schedule_from_to(lkey, requester, iter->requester);
                        timer_grant_begin(lkey, iter->requester);
                        timer_grant_local(lkey, iter->requester);
                    } else {
                        // this is a remote waiter
                        net_send(iter->host, POST_LOCK_REPLY, iter->op, lkey, iter->host, iter->requester);
                        timer_schedule_from_to(lkey, requester, iter->requester);
                        count_server_grant();
                    }
#ifndef PARLOCK_ENQUEUE_HOLDERS
                    free(iter);
#endif
                }
                unblock_lock_queue(lkey);
            } else {
                // the queue is empty
                unblock_lock_queue(lkey);
            }
        } else {
            // abort release
            unblock_lock_queue(lkey);
        }
    } else if (op == LOCK_SHARED) {
        // shared release
        // DEBUG("lcore %u requester %u shared release lock %u", rte_lcore_id(), requester, lkey);
        if (get_lock_state(lkey) == STATE_SHARED && !ret) {
            // holder release lock
            sub_shared_counter(lkey);
            if (get_shared_counter(lkey) > 0) {
                // if there are still shared holders
                // do not free the lock, just return
                unblock_lock_queue(lkey);
                return;
            }
            // else free the lock
            set_lock_state(lkey, STATE_FREE);
            lock_queue_entry *head = get_head_request(lkey);
            if (head != NULL && head->op == LOCK_EXCL) {
                // DEBUG("lcore %u switched lock %u to exclusive state", rte_lcore_id(), lkey);
#ifndef PARLOCK_ENQUEUE_HOLDERS
                dequeue_request(lkey);
#endif
                set_lock_state(lkey, STATE_EXCLUSIVE);
                uint32_t tid = head->requester;
                if (head->host == LOCALHOST_ID) {
                    // this is a local waiter
                    coroutine_wait_queue_signal(lkey, tid);
                    timer_schedule_from_to(lkey, requester, tid);
                    timer_grant_begin(lkey, tid);
                    timer_grant_local(lkey, tid);
                } else {
                    // this is a remote waiter
                    net_send(head->host, POST_LOCK_REPLY, head->op, lkey, head->host, head->requester);
                    timer_schedule_from_to(lkey, requester, head->requester);
                    count_server_grant();
                }
#ifndef PARLOCK_ENQUEUE_HOLDERS
                free(head);
#endif
                unblock_lock_queue(lkey);
            } else if (head != NULL && head->op == LOCK_SHARED) {
                unblock_lock_queue(lkey);
                ERROR("lcore %u Abnormal condition in shared local unlock: shared waiter %u in the queue when state is shared", rte_lcore_id(), head->requester);
            } else {
                // empty queue
                unblock_lock_queue(lkey); 
            }
        } else if (get_lock_state(lkey) == STATE_SHARED) {
            // shared abort release when lock state is shared
            unblock_lock_queue(lkey);
            ERROR("lcore %u Abnormal condition in shared local unlock %u when state is shared: unlocker %u abort release", rte_lcore_id(), lkey, requester);
        } else {
            // abort release when lock state is exclusive
            unblock_lock_queue(lkey);
        }
    } else {
        unblock_lock_queue(lkey);
    }
#else
    lock_queue_entry *cur_head = get_head_request(lkey);
    if (cur_head->requester != requester) {
        int ret = erase_lock_request(lkey, requester, LOCALHOST_ID, op);
        if (ret == 1) {
            if (op == LOCK_EXCL) {
                sub_exclusive_counter(lkey);
            } else if (op == LOCK_SHARED) {
                sub_shared_counter(lkey);
            }
        }
        unblock_lock_queue(lkey);
        return;
    }

    // get current lock holder
    lock_queue_entry *lqe = dequeue_request(lkey); // TODO: not correct?
    if (lqe->op == LOCK_EXCL) {
        // exclusive release
        sub_exclusive_counter(lkey);
        lock_queue_entry *head = get_head_request(lkey);
        if (head != NULL && head->op == LOCK_EXCL) {
            uint32_t tid = head->requester;
            unblock_lock_queue(lkey);
            free(lqe);
            if (head->host == LOCALHOST_ID) {
                // this is a local waiter
                coroutine_wait_queue_signal(lkey, tid);
                timer_schedule_from_to(lkey, requester, tid);
                timer_grant_begin(lkey, tid);
                timer_grant_local(lkey, tid);
            } else {
                // this is a remote waiter
                net_send(head->host, POST_LOCK_REPLY, head->op, lkey, head->host, head->requester);
                timer_schedule_from_to(lkey, requester, head->requester);
                count_server_grant();
            }
        } else if (head != NULL && head->op == LOCK_SHARED) {
            auto shared_head_ptrs = get_head_shared_requests(lkey);
            std::vector<lock_queue_entry> shared_head;
            for (auto iter : shared_head_ptrs) shared_head.push_back(*iter);
            unblock_lock_queue(lkey);
            free(lqe);

            for (auto iter : shared_head) {
                if (iter.host == LOCALHOST_ID) {
                    // this is a local waiter
                    coroutine_wait_queue_signal(lkey, iter.requester);
                    timer_schedule_from_to(lkey, requester, iter.requester);
                    timer_grant_begin(lkey, iter.requester);
                    timer_grant_local(lkey, iter.requester);
                } else {
                    // this is a remote waiter
                    net_send(iter.host, POST_LOCK_REPLY, iter.op, lkey, iter.host, iter.requester);
                    timer_schedule_from_to(lkey, requester, iter.requester);
                    count_server_grant();
                }
            }
        } else {
            unblock_lock_queue(lkey);
            free(lqe);
        }
    } else if (lqe->op == LOCK_SHARED) {
        // shared release
        sub_shared_counter(lkey);
        lock_queue_entry *head = get_head_request(lkey);
        free(lqe);
        if (head != NULL && head->op == LOCK_EXCL) {
            uint32_t tid = head->requester;
            unblock_lock_queue(lkey);

            // if the waiter if exclusive, grant the lock
            if (head->host == LOCALHOST_ID) {
                // this is a local waiter
                coroutine_wait_queue_signal(lkey, tid);
                timer_schedule_from_to(lkey, requester, tid);
                timer_grant_begin(lkey, tid);
                timer_grant_local(lkey, tid);
            } else {
                // this is a remote waiter
                net_send(head->host, POST_LOCK_REPLY, head->op, lkey, head->host, head->requester);
                timer_schedule_from_to(lkey, requester, head->requester);
                count_server_grant();
            }
        } else unblock_lock_queue(lkey);
    } else {
        unblock_lock_queue(lkey);
        free(lqe);
    }
#endif
}

void release_lock_remote(uint32_t lkey, uint32_t requester, lock_state op) {
    net_send(get_lock_server(lkey), POST_UNLOCK_REQ, op, lkey, LOCALHOST_ID, requester);
}