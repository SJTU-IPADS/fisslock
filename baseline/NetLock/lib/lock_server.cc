#include "lock_server.h"
#include "statistics.h"
#include "lock.h"
#include "debug.h"

int *len_in_switch = NULL;

/*
    This function will free `node` or enqueue `node`
*/
void process_primary_backup(uint32_t lcore_id, lock_queue_node *node) {
    count_primary();
    if (node->op_type == ACQUIRE_LOCK) {
        timer_since_burst_handle_acquire_begin(node->lock_id, node->txn_id);
        process_primary_acquire(lcore_id, node);
    } else if (node->op_type == RELEASE_LOCK) {
        timer_since_burst_schedule_start(node->lock_id, node->txn_id);
        process_primary_release(lcore_id, node);
    } else {
        free(node);
    }
}
/*
    This funtction will free `node` or enqueue `node`
*/
void process_secondary_backup(uint32_t lcore_id, lock_queue_node *node) {
    count_secondary();
    if (node->op_type == ACQUIRE_LOCK || node->op_type == PUSH_BACK_LOCK) {
        process_secondary_acquire(lcore_id, node);
    } else if (node->op_type == RELEASE_LOCK) {
        process_secondary_release(lcore_id, node);
    } else {
        free(node);
    }
}

/*
    This function enqueue `node`
*/
static void process_primary_acquire(uint32_t lcore_id, lock_queue_node *node) {
    count_primary_acquire();
    block_queue(node->lock_id);
    enqueue_request(node);
    if (node->mode == SHARED) {
        // LOG("[host%u core%u]txn %u acquire shared lock %u", LOCALHOST_ID, lcore_id, node->txn_id, node->lock_id);
        if (get_lock_state(node->lock_id) == STATE_FREE || get_lock_state(node->lock_id) == STATE_SHARED) {
            // grant shared lock
            set_lock_state(node->lock_id, STATE_SHARED);
            add_shared_counter(node->lock_id);
            net_send_lock_server(lcore_id, node, GRANT_LOCK_FROM_SERVER, 0, get_seqnum(node->lock_id));
            // LOG("[host%u core%u]host %u txn %u granted shared lock %u", LOCALHOST_ID, lcore_id, node->client_id, node->txn_id, node->lock_id);
            timer_handle_acquire_end(node->lock_id, node->txn_id);
            unblock_queue(node->lock_id);
            count_primary_grant();
        } else {
            // enqueue shared lock
            timer_queue_start(node->lock_id, node->txn_id);
            unblock_queue(node->lock_id);
        }
    } else if (node->mode == EXCLUSIVE) {
        // LOG("[host%u core%u]txn %u acquire exclusive lock %u", LOCALHOST_ID, lcore_id, node->txn_id, node->lock_id);
        if (get_lock_state(node->lock_id) == STATE_FREE) {
            // grant exclusive lock
            set_lock_state(node->lock_id, STATE_EXCLUSIVE);
            net_send_lock_server(lcore_id, node, GRANT_LOCK_FROM_SERVER, 0, get_seqnum(node->lock_id));
            // LOG("[host%u core%u]host %u txn %u granted exclusive lock %u", LOCALHOST_ID, lcore_id, node->client_id, node->txn_id, node->lock_id);
            timer_handle_acquire_end(node->lock_id, node->txn_id);
            unblock_queue(node->lock_id);
            count_primary_grant(); 
        } else {
            // enqueue exclusive lock
            timer_queue_start(node->lock_id, node->txn_id);
            unblock_queue(node->lock_id);
        }
    } else {
        unblock_queue(node->lock_id);
    }
}

/*
    This function will free `node`
*/
static void process_primary_release(uint32_t lcore_id, lock_queue_node *node) {
    count_primary_release();
    block_queue(node->lock_id);
    int ret = erase_lock_request(node->lock_id, node->txn_id, node->client_id, node->mode);
    if (node->mode == SHARED) {
        // LOG("[host%u core%u]txn %u release shared lock %u", LOCALHOST_ID, lcore_id, node->txn_id, node->lock_id);
        if (get_lock_state(node->lock_id) == STATE_SHARED) {
            // shared holder release lock
            sub_shared_counter(node->lock_id);
            if (get_shared_counter(node->lock_id) > 0) {
                // LOG("[host%u core%u]txn %u release shared lock %u, still SHARED, %d holders", 
                    // LOCALHOST_ID, lcore_id, node->txn_id, node->lock_id, get_shared_counter(node->lock_id));
                // if there are other shared holders now, just return
                unblock_queue(node->lock_id);
                free(node);
                return;
            }
            // schedule
            set_lock_state(node->lock_id, STATE_FREE);
            lock_queue_node* cur_head = get_head_request(node->lock_id);
            if (cur_head != NULL && cur_head->mode == EXCLUSIVE) {
                // schedule to exclusive waiter
                set_lock_state(node->lock_id, STATE_EXCLUSIVE);
                cur_head->payload = node->payload;
                cur_head->payload_size = node->payload_size;
                net_send_lock_server(lcore_id, cur_head, GRANT_LOCK_FROM_SERVER, 1, get_seqnum(node->lock_id));
                // LOG("[host%u core%u]host %u txn %u granted exclusive lock %u", LOCALHOST_ID, lcore_id, node->client_id, node->txn_id, node->lock_id);
                timer_schedule_from_to(node->lock_id, node->txn_id, cur_head->txn_id);
                timer_queue_end(node->lock_id, cur_head->txn_id);
                unblock_queue(node->lock_id);
                count_primary_grant();
            } else if (cur_head != NULL && cur_head->mode == SHARED) {
                unblock_queue(node->lock_id);
            } else {
                // net_broadcast_lock_server(lcore_id, node, MEM_DIFF);
                // LOG("[host%u core%u]txn %u set FREE shared lock %u", LOCALHOST_ID, lcore_id, node->txn_id, node->lock_id);
                add_seqnum(node->lock_id);
                unblock_queue(node->lock_id);
            }
        } else {
            // abort release
            unblock_queue(node->lock_id);
        }
    } else if (node->mode == EXCLUSIVE) {
        // LOG("[host%u core%u]txn %u release exclusive lock %u", LOCALHOST_ID, lcore_id, node->txn_id, node->lock_id);
        if (get_lock_state(node->lock_id) == STATE_EXCLUSIVE && !ret) {
            // exclusive holder release lock
            set_lock_state(node->lock_id, STATE_FREE);
            lock_queue_node* cur_head = get_head_request(node->lock_id);
            if (cur_head != NULL && cur_head->mode == EXCLUSIVE) {
                // schedule to exclusive waiter
                set_lock_state(node->lock_id, STATE_EXCLUSIVE);
                cur_head->payload = node->payload;
                cur_head->payload_size = node->payload_size;
                net_send_lock_server(lcore_id, cur_head, GRANT_LOCK_FROM_SERVER, 1, get_seqnum(node->lock_id));
                // LOG("[host%u core%u]host %u txn %u granted exclusive lock %u", LOCALHOST_ID, lcore_id, cur_head->client_id, cur_head->txn_id, cur_head->lock_id);
                timer_schedule_from_to(node->lock_id, node->txn_id, cur_head->txn_id);
                timer_queue_end(node->lock_id, cur_head->txn_id);
                unblock_queue(node->lock_id);
                count_primary_grant();
            } else if (cur_head != NULL && cur_head->mode == SHARED) {
                // schedule to shared waiters
                set_lock_state(node->lock_id, STATE_SHARED);
                std::vector<lock_queue_node*> shared_head = get_shared_requests(node->lock_id);
                for (auto& iter : shared_head) {
                    add_shared_counter(node->lock_id);
                    iter->payload = node->payload;
                    iter->payload_size = node->payload_size;
                    net_send_lock_server(lcore_id, iter, GRANT_LOCK_FROM_SERVER, 1, get_seqnum(node->lock_id));
                    // LOG("[host%u core%u]host%u txn %u granted shared lock %u", LOCALHOST_ID, lcore_id, iter->client_id, iter->txn_id, iter->lock_id);
                    timer_schedule_from_to(node->lock_id, node->txn_id, iter->txn_id);
                    timer_queue_end(node->lock_id, iter->txn_id);
                    count_primary_grant();
                }
                unblock_queue(node->lock_id);
            } else {
                // net_broadcast_lock_server(lcore_id, node, MEM_DIFF);
                add_seqnum(node->lock_id);
                unblock_queue(node->lock_id);
            }
        } else {
            // abort release
            unblock_queue(node->lock_id);
        }
    } else {
        unblock_queue(node->lock_id);
    }
   free(node);
}

/*
    This function will enqueue `node`
*/
static void process_secondary_acquire(uint32_t lcore_id, lock_queue_node *node) {
    timer_secondary_begin(node->lock_id, node->txn_id);
    if (node->op_type == ACQUIRE_LOCK) {
        count_secondary_acquire();
    }
    block_queue(node->lock_id);
    enqueue_request(node);
    unblock_queue(node->lock_id);
}

/*
    This function will free `node`
*/
static void process_secondary_release(uint32_t lcore_id, lock_queue_node *node) {
    count_secondary_release();
    block_queue(node->lock_id);
    int len_in_server = get_lock_queue_size(node->lock_id);
    int len_on_switch = len_in_switch[node->lock_id];
    int dequeue_length = (len_in_server < len_on_switch) ? len_in_server : len_on_switch;

    for (int node_num = 0; node_num < dequeue_length; node_num++) {
        lock_queue_node *dequeued_node = get_request(node->lock_id,  node_num);
        net_send_lock_server(lcore_id, dequeued_node, PUSH_BACK_LOCK, 0, get_seqnum(node->lock_id));
        count_secondary_push_back();
    }
    for (int node_num = 0; node_num < dequeue_length; node_num++) {
        lock_queue_node *dequeued_node = dequeue_request(node->lock_id);
        timer_secondary_end(dequeued_node->lock_id, dequeued_node->txn_id);
        free(dequeued_node);
    }
    unblock_queue(node->lock_id);
    free(node);
}
