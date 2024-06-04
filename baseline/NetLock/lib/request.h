#ifndef __NETLOCK_REQUEST_H
#define __NETLOCK_REQUEST_H

#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <vector>

#include "lock.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_lock_queue(uint32_t lock_id);
void init_all_locks();
void enqueue_request(lock_queue_node* node);
lock_queue_node* dequeue_request(uint32_t lock_id);
lock_queue_node* get_head_request(uint32_t lock_id);
lock_queue_node* get_request(uint32_t lock_id, int index);
std::vector<lock_queue_node*> get_shared_requests(uint32_t lock_id);
void block_queue(uint32_t lock_id);
void unblock_queue(uint32_t lock_id);
int get_ticket_id(uint32_t lock_id);
int get_shared_counter(uint32_t lock_id);
int get_exclusive_counter(uint32_t lock_id);
int get_lock_state(uint32_t lock_id);
void set_lock_state(uint32_t lock_id, int lock_state_);
void add_shared_counter(uint32_t lock_id);
void sub_shared_counter(uint32_t lock_id);
void add_exclusive_counter(uint32_t lock_id);
void sub_exclusive_counter(uint32_t lock_id);
int get_lock_queue_size(uint32_t lock_id);
int erase_lock_request(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op);
void add_seqnum(uint32_t lock_id);
int get_seqnum(uint32_t lock_id);

lock_queue_node* mbuf_to_node(struct rte_mbuf *mbuf);

#ifdef __cplusplus
}
#endif

#endif