#include <deque>
#include <unordered_map>
#include <rte_malloc.h>
#include "lock_queue.h"
#include "conf.h"
#include "net.h"
#include "statistics.h"
#include "debug.h"

#ifdef PARLOCK_READER_FIRST
#ifdef PARLOCK_ENQUEUE_HOLDERS
#define PARLOCK_READER_FIRST_ENQUEUE_HOLDERS
#endif
#endif

class LockTable {
private:
    struct LockQueue {
        std::deque<lock_queue_entry*> request_queue;
        pthread_mutex_t queue_mutex;
        int shared_counter;
        int exclusive_counter;
        int lock_state;
    };
    std::unordered_map<uint32_t, LockQueue*> queue_per_lock;
    pthread_mutex_t huge_lock;
public:
    LockTable() {
        pthread_mutex_init(&huge_lock, NULL);
    }
    ~LockTable() {}
    void init_lock(uint32_t lock_id) {
        pthread_mutex_lock(&huge_lock);
        if (queue_per_lock.find(lock_id) == queue_per_lock.end()) {
            LockQueue *lock_queue = new LockQueue();
            lock_queue->shared_counter = 0;
            lock_queue->exclusive_counter = 0;
            lock_queue->lock_state = STATE_FREE;
            pthread_mutex_init(&(lock_queue->queue_mutex), NULL);
            queue_per_lock[lock_id] = lock_queue;
        }
        pthread_mutex_unlock(&huge_lock);
    }
    void block_lock_queue(uint32_t lock_id) {
        pthread_mutex_lock(&(queue_per_lock[lock_id]->queue_mutex));
    }
    void unblock_lock_queue(uint32_t lock_id) {
        pthread_mutex_unlock(&(queue_per_lock[lock_id]->queue_mutex));
    }
    int get_shared_counter(uint32_t lock_id) {
        return queue_per_lock[lock_id]->shared_counter;
    }
    int get_exclusive_counter(uint32_t lock_id) {
        return queue_per_lock[lock_id]->exclusive_counter;
    }
    int get_lock_state(uint32_t lock_id) {
        return queue_per_lock[lock_id]->lock_state;
    }
    void set_lock_state(uint32_t lock_id, int lock_state_) {
        queue_per_lock[lock_id]->lock_state = lock_state_;
    }
    void add_shared_counter(uint32_t lock_id) {
        queue_per_lock[lock_id]->shared_counter += 1;
    }
    void sub_shared_counter(uint32_t lock_id) {
        queue_per_lock[lock_id]->shared_counter -= 1;
    }
    void add_exclusive_counter(uint32_t lock_id) {
        queue_per_lock[lock_id]->exclusive_counter += 1;
    }
    void sub_exclusive_counter(uint32_t lock_id) {
        queue_per_lock[lock_id]->exclusive_counter -= 1;
    }
    void enqueue_request(uint8_t op, uint32_t lock_id, uint8_t machine_id, uint32_t lh_id) {
        lock_queue_entry *lqe = (lock_queue_entry*)malloc(sizeof(lock_queue_entry));
        lqe->host = machine_id;
        lqe->op = op;
        lqe->requester = lh_id;
        queue_per_lock[lock_id]->request_queue.emplace_back(lqe);
#ifdef PARLOCK_TRACE_LOCK_QUEUE_SIZE
       inc_lock_queue_size();
#endif
    }
    lock_queue_entry* dequeue_request(uint32_t lock_id) {
        lock_queue_entry *result = NULL;
        if (!(queue_per_lock[lock_id]->request_queue.empty())) {
            result = queue_per_lock[lock_id]->request_queue.front();
            queue_per_lock[lock_id]->request_queue.pop_front();
#ifdef PARLOCK_TRACE_LOCK_QUEUE_SIZE
            dec_lock_queue_size();
#endif
        }
        return result;
    }
    lock_queue_entry* get_head_request(uint32_t lock_id) {
        lock_queue_entry *result = NULL;
        if (!(queue_per_lock[lock_id]->request_queue.empty())) {
            result = queue_per_lock[lock_id]->request_queue.front();
        }
        return result;
    }
    bool is_lock_holder(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op) {
        if (queue_per_lock[lock_id]->request_queue.empty()) return false;
        lock_queue_entry* head = queue_per_lock[lock_id]->request_queue.front();
        if (head->op == LOCK_EXCL) return host == head->host && requester == head->requester && op == head->op;
        else {
            for (auto& iter : queue_per_lock[lock_id]->request_queue) {
                if (iter->op == LOCK_EXCL) return false;
                if (host == iter->host && requester == iter->requester && op == iter->op) return true;
            }
            return false;
        }
    }
    std::vector<lock_queue_entry*> get_head_shared_requests(uint32_t lock_id) {
        std::vector<lock_queue_entry*> result;
        for (auto iter : queue_per_lock[lock_id]->request_queue) {
            if (iter->op == LOCK_SHARED) {
                result.emplace_back(iter);
            } else {
                break;
            }
        }
        return result;
    }
    std::vector<lock_queue_entry*> dequeue_shared_requests(uint32_t lock_id) {
        std::vector<lock_queue_entry*> result;
        auto iter = queue_per_lock[lock_id]->request_queue.begin();
        int prev_idx = -1;
        while (iter != queue_per_lock[lock_id]->request_queue.end()) {
            if ((*iter)->op == LOCK_SHARED) {
                result.emplace_back(*iter);
                queue_per_lock[lock_id]->request_queue.erase(iter);
#ifdef PARLOCK_TRACE_LOCK_QUEUE_SIZE
                dec_lock_queue_size();
#endif
                if (queue_per_lock[lock_id]->request_queue.empty()) return result;
                iter = queue_per_lock[lock_id]->request_queue.begin() + prev_idx + 1;
            } else {
                iter++;
                prev_idx++;
            }
        }
        return result;
    }
    std::vector<lock_queue_entry*> get_shared_requests(uint32_t lock_id) {
        std::vector<lock_queue_entry*> result;
        for (auto& it : queue_per_lock[lock_id]->request_queue) {
            if (it->op == LOCK_SHARED) {
                result.emplace_back(it);
            }
        }
        return result;
    }
    int erase_lock_request(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op) {
#ifdef PARLOCK_READER_FIRST_ENQUEUE_HOLDERS
        int idx = 0;
        for (auto iter = queue_per_lock[lock_id]->request_queue.begin(); 
            iter != queue_per_lock[lock_id]->request_queue.end(); iter++) {
            if ((*iter)->host == host && (*iter)->requester == requester && (*iter)->op == op) {
                free(*iter);
                queue_per_lock[lock_id]->request_queue.erase(iter);
#ifdef PARLOCK_TRACE_LOCK_QUEUE_SIZE
                dec_lock_queue_size();
#endif
                // when using reader first policy,
                // every  shared requester in the queue is 
                // lock holder
                if (op == LOCK_SHARED) {
                    return 0;
                // when using reader first policy,
                // only the first exclusive requester in the queue
                // is lock holder
                } else {
                    return idx;
                }
            }
            idx++;
        }
        return -1;
#else
        for (auto iter = queue_per_lock[lock_id]->request_queue.begin(); 
            iter != queue_per_lock[lock_id]->request_queue.end(); iter++) {
            if ((*iter)->host == host && (*iter)->requester == requester && (*iter)->op == op) {
                free(*iter);
                queue_per_lock[lock_id]->request_queue.erase(iter);
#ifdef PARLOCK_TRACE_LOCK_QUEUE_SIZE
                dec_lock_queue_size();
#endif
                return 1;
            }
        }
        return 0;
#endif
    }
};

LockTable lock_table;

void enqueue_request(uint8_t op, uint32_t lock_id, uint8_t machine_id, uint32_t lh_id) {
    lock_table.enqueue_request(op, lock_id, machine_id, lh_id);
}

lock_queue_entry* dequeue_request(uint32_t lock_id) {
    return lock_table.dequeue_request(lock_id);
}

lock_queue_entry* get_head_request(uint32_t lock_id) {
    return lock_table.get_head_request(lock_id);
}

bool is_lock_holder(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op) {
    return lock_table.is_lock_holder(lock_id, requester, host, op);
}

std::vector<lock_queue_entry*> get_head_shared_requests(uint32_t lock_id) {
    return lock_table.get_head_shared_requests(lock_id);
}

std::vector<lock_queue_entry*> dequeue_shared_requests(uint32_t lock_id) {
    return lock_table.dequeue_shared_requests(lock_id);
}

std::vector<lock_queue_entry*> get_shared_requests(uint32_t lock_id) {
    return lock_table.get_shared_requests(lock_id);
}

int get_shared_counter(uint32_t lock_id) {
    return lock_table.get_shared_counter(lock_id);
}

int get_exclusive_counter(uint32_t lock_id) {
    return lock_table.get_exclusive_counter(lock_id);
}

int get_lock_state(uint32_t lock_id) {
    return lock_table.get_lock_state(lock_id);
}

void set_lock_state(uint32_t lock_id, int lock_state_) {
    lock_table.set_lock_state(lock_id, lock_state_);
}

void add_shared_counter(uint32_t lock_id) {
    lock_table.add_shared_counter(lock_id);
}

void sub_shared_counter(uint32_t lock_id) {
    lock_table.sub_shared_counter(lock_id);
}
void add_exclusive_counter(uint32_t lock_id) {
    lock_table.add_exclusive_counter(lock_id);
}

void sub_exclusive_counter(uint32_t lock_id) {
    lock_table.sub_exclusive_counter(lock_id);
}

void block_lock_queue(uint32_t lock_id) {
    lock_table.block_lock_queue(lock_id);
}

void unblock_lock_queue(uint32_t lock_id) {
    lock_table.unblock_lock_queue(lock_id);
}

void init_lock(uint32_t lock_id) {
    lock_table.init_lock(lock_id);
}

void init_all_locks() {
#ifdef PARLOCK_TPCC_SHUFFLE
    for (uint32_t lock_id = 0; lock_id <= MAX_LOCK_NUM; lock_id++) {
        if (((lock_id % HOST_NUM) + 1) != LOCALHOST_ID) {
            continue;
        }
        init_lock(lock_id);
    }
#else
#ifdef PARLOCK_ZIPF_LOAD_BALANCE
    uint32_t lock_begin = lock_map[(LOCALHOST_ID - 1) * 2];
    uint32_t lock_end = lock_map[(LOCALHOST_ID - 1) * 2 + 1];
#else
    int lock_per_host = MAX_LOCK_NUM / HOST_NUM;
    uint32_t lock_begin = lock_per_host * (LOCALHOST_ID - 1);
    uint32_t lock_end = lock_per_host * LOCALHOST_ID - 1;
#endif
    for (uint32_t lock_id = lock_begin; lock_id <= lock_end; lock_id++) {
        init_lock(lock_id);
    }
#endif
}

int erase_lock_request(uint32_t lock_id, uint32_t requester, uint8_t host, uint8_t op) {
    return lock_table.erase_lock_request(lock_id, requester, host, op);
}
