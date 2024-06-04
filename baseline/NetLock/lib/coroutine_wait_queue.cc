#include <unordered_map>
#include <rte_malloc.h>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "coroutine_wait_queue.h"
#include "statistics.h"
#include "conf.h"
#include "debug.h"

using namespace std::chrono_literals;

struct LockEntry {
    std::mutex mtx;
#if LOCK_WAIT_MECHANISM == LOCK_WAIT_COND_VAR
    std::condition_variable wait_cv;
#endif
    std::unordered_map<uint32_t, bool> wait_flag;
};
std::unordered_map<uint32_t, LockEntry*> lock_wait_table;

void init_lock_wait_table(uint32_t lock_id) {
    LockEntry* le = new LockEntry();
    lock_wait_table.insert({lock_id, le});
}

void add_to_wait_queue(uint32_t lcore_id, uint32_t lock_id, uint32_t requester) {
    auto lock_pos = lock_wait_table.find(lock_id);
    if (unlikely(lock_pos == lock_wait_table.end())) {
        ERROR("coroutine wait queue cannot find lock %u in lock table", lock_id);
    }
    std::unique_lock<std::mutex> lk(lock_pos->second->mtx);
    lock_pos->second->wait_flag.insert({requester, false});
}


void coroutine_wait_queue_signal(uint32_t lcore_id, uint32_t lock_id, uint32_t requester) {
    auto lock_pos = lock_wait_table.find(lock_id);
    if (unlikely(lock_pos == lock_wait_table.end())) {
        ERROR("coroutine wait queue cannot find lock %u in lock table", lock_id);
    }
    std::unique_lock<std::mutex> lk(lock_pos->second->mtx);
    auto requester_pos = lock_pos->second->wait_flag.find(requester);
    if (unlikely(requester_pos == lock_pos->second->wait_flag.end())) {
        ERROR("coroutine wait queue cannot find pair <%u,%u>", lock_id, requester);
    }
    requester_pos->second = true;
#if LOCK_WAIT_MECHANISM == LOCK_WAIT_COND_VAR
    lock_pos->second->wait_cv.notify_all();
#endif
}

bool lock_req_granted(lock_req req, uint32_t lid, uint32_t tid) {
    uint32_t lock_id = ((req >> 32) & (uint32_t)(0xffffffff));
    uint32_t requester = (req & (uint32_t)(0xffffffff));
    auto lock_pos = lock_wait_table.find(lock_id);
    if (unlikely(lock_pos == lock_wait_table.end())) {
        ERROR("coroutine wait queue cannot find lock %u in lock table", lock_id);
    }
    std::unique_lock<std::mutex> lk(lock_pos->second->mtx);
    auto requester_pos = lock_pos->second->wait_flag.find(requester);
    if (unlikely(requester_pos == lock_pos->second->wait_flag.end())) {
        ERROR("coroutine wait queue cannot find pair <%u,%u>", lock_id, requester);
    }
#if LOCK_WAIT_MECHANISM == LOCK_WAIT_COND_VAR
    if (requester_pos->second) {
        return true;
    }
    auto notified = &(requester_pos->second);
    if (lock_pos->second->wait_cv.wait_for(
        lk, 5us, [notified](){return *notified;})) {
        *notified = false;
        return true;
    } else {
        return false;
    }
#else
    bool result = requester_pos->second;
    if (result) requester_pos->second = false;
    return result;
#endif
}