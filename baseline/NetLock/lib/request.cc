#include <deque>
#include <mutex>
#include <unordered_map>
#include <pthread.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>

#include "request.h"
#include "lock_server.h"
#include "lock.h"
#include "debug.h"

using std::deque;
using std::mutex;

/* 
    RequestQueue:
    Queue the lock requests per lock.
    When access a node queue of a lock, should always call `block_queue` first.
*/
struct NodeQueuePerLock {
    std::deque<lock_queue_node*> node_queue;
    std::mutex queue_mutex;
    int shared_counter;
    int exclusive_counter;
    int lock_state;
    int seqnum;
    int ticket_id;
};
std::unordered_map<uint32_t, NodeQueuePerLock*> lock_queue;
    

void init_lock_queue(uint32_t lock_id) {
    if (lock_queue.find(lock_id) == lock_queue.end()) {
        NodeQueuePerLock *node_queue_per_lock = new NodeQueuePerLock();
        node_queue_per_lock->shared_counter = 0;
        node_queue_per_lock->exclusive_counter = 0;
        node_queue_per_lock->lock_state = FREE;
        node_queue_per_lock->seqnum = 0;
        node_queue_per_lock->ticket_id = 0;
        lock_queue[lock_id] = node_queue_per_lock;
    }
}

void enqueue_request(lock_queue_node *node) {
    uint32_t lock_id = node->lock_id;
    if (lock_queue.find(lock_id) == lock_queue.end()) {
        ERROR("<%s:%d> lock %u not in the lock queue", __FILE__, __LINE__, lock_id);
    }
    lock_queue[lock_id]->node_queue.push_back(node);
}

lock_queue_node* dequeue_request(uint32_t lock_id) {
    lock_queue_node *result = NULL;
    if (!lock_queue[lock_id]->node_queue.empty()) {
        result = lock_queue[lock_id]->node_queue.front();
        lock_queue[lock_id]->node_queue.pop_front();
    }
    return result;
}

lock_queue_node* get_head_request(uint32_t lock_id) {
    lock_queue_node *result = NULL;
    if (!lock_queue[lock_id]->node_queue.empty()) {
        result = lock_queue[lock_id]->node_queue.front();
    }
    return result;
}

std::vector<lock_queue_node*> get_shared_requests(uint32_t lock_id) {
    std::vector<lock_queue_node*> result;
    for (auto iter : lock_queue[lock_id]->node_queue) {
        if (iter->mode == SHARED) {
            result.push_back(iter);
        }    
    }
    return result;
}

lock_queue_node* get_request(uint32_t lock_id, int index) {
    return lock_queue[lock_id]->node_queue[index];
}

int get_shared_counter(uint32_t lock_id) {
    return lock_queue[lock_id]->shared_counter;
}

int get_exclusive_counter(uint32_t lock_id) {
    return lock_queue[lock_id]->exclusive_counter;
}

int get_lock_state(uint32_t lock_id) {
    return lock_queue[lock_id]->lock_state;
}

void set_lock_state(uint32_t lock_id, int lock_state_) {
    lock_queue[lock_id]->lock_state = lock_state_;
}

void add_shared_counter(uint32_t lock_id) {
    lock_queue[lock_id]->shared_counter += 1;
}

void sub_shared_counter(uint32_t lock_id) {
    lock_queue[lock_id]->shared_counter -= 1;
}

void add_exclusive_counter(uint32_t lock_id) {
    lock_queue[lock_id]->exclusive_counter += 1;
}

void sub_exclusive_counter(uint32_t lock_id) {
    lock_queue[lock_id]->exclusive_counter -= 1;
}

void block_queue(uint32_t lock_id) {
    if (lock_queue.find(lock_id) == lock_queue.end()) {
        ERROR("<%s:%d> lock %u not in the lock queue", __FILE__, __LINE__, lock_id);
    }
    lock_queue[lock_id]->queue_mutex.lock();
    lock_queue[lock_id]->ticket_id += 1;
}

void unblock_queue(uint32_t lock_id) {
    if (lock_queue.find(lock_id) == lock_queue.end()) {
        ERROR("<%s:%d> lock %u not in the lock queue", __FILE__, __LINE__, lock_id);
    }
    lock_queue[lock_id]->queue_mutex.unlock();
}

int get_ticket_id(uint32_t lock_id) {
    return lock_queue[lock_id]->ticket_id;
}

int get_lock_queue_size(uint32_t lock_id) {
    return lock_queue[lock_id]->node_queue.size();
}

int erase_lock_request(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op) {
    int cur_count = 0;
    for(auto iter = lock_queue[lock_id]->node_queue.begin();
    iter != lock_queue[lock_id]->node_queue.end();
    iter++) {
        if ((*iter)->client_id == host && (*iter)->lock_id == lock_id && (*iter)->txn_id == requester && (*iter)->mode == op) {
            free(*iter);
            lock_queue[lock_id]->node_queue.erase(iter);
            return cur_count;
        }
        cur_count++;
    }
    return 0;
}

void add_seqnum(uint32_t lock_id) {
    lock_queue[lock_id]->seqnum += 1;
}

int get_seqnum(uint32_t lock_id) {
    return lock_queue[lock_id]->seqnum;
}

void init_all_locks() {
    for (uint32_t lock_id = 0; lock_id < MAX_LOCK_NUM; lock_id++) {
        init_lock_queue(lock_id);
    }
}

/*
    This is a util funciton, it reads the fields of `mbuf`,
    allocate a `node`, and fills the fields into `node`.
    Note that `mbuf` will not be freed inside this function.
    Note that this function will call `ntoh` to change endianness.
    Note that this function will return NULL, if the parse fails.
*/
lock_queue_node* mbuf_to_node(struct rte_mbuf *mbuf) {
    rte_prefetch0(rte_pktmbuf_mtod(mbuf, void *));
    struct rte_ether_hdr* eth = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)((uint8_t*) eth + sizeof(struct rte_ether_hdr));
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)((uint8_t*) ip + sizeof(struct rte_ipv4_hdr));
    lock_post_header* message_header = (lock_post_header*) ((uint8_t *) eth + 
                                    sizeof(struct rte_ether_hdr) + 
                                    sizeof(struct rte_ipv4_hdr) + 
                                    sizeof(struct rte_udp_hdr));
    struct rte_ether_addr *src_addr, *dst_addr;
    src_addr = &(eth->src_addr);
    dst_addr = &(eth->dst_addr);
    uint8_t mem_role = dst_addr->addr_bytes[5];
    uint32_t ip_from_eth = src_addr->addr_bytes[2] | 
                            (src_addr->addr_bytes[3] << 8) | 
                            (src_addr->addr_bytes[4] << 16) | 
                            (src_addr->addr_bytes[5] << 24);
    lock_queue_node *result = (lock_queue_node*)malloc(sizeof(lock_queue_node));
    result->client_id = message_header->client_id;
    result->ip_src = ip_from_eth; // ip_src will not be used in processing, so not change its endianness
    result->lock_id = ntohl(message_header->lock_id);
    result->mode = message_header->mode;
    result->op_type = message_header->op_type;
    result->role = mem_role;
    result->txn_id = ntohl(message_header->txn_id);
    result->payload = NULL;
    result->payload_size = 0;

    // Get the payload if it exists
    size_t header_size = sizeof(struct rte_ether_hdr) + 
                        sizeof(struct rte_ipv4_hdr) + 
                        sizeof(struct rte_udp_hdr) +
                        sizeof(lock_post_header);
    if (mbuf->data_len > header_size) {
        void* payload_ptr = (void*)((uint8_t*)message_header + sizeof(lock_post_header));
        result->payload_size = mbuf->data_len - header_size;
        result->payload = (void*)malloc(result->payload_size);
        memcpy(result->payload, payload_ptr, result->payload_size);
    }

    return result;
}