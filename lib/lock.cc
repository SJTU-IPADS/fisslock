
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <vector>
#include <atomic>
#include <queue>
#include <list>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include <arpa/inet.h>
#include <unistd.h>

using namespace std;
using namespace chrono;
using namespace chrono_literals;

#include "lock.h"
#include "statistics.h"
#include "debug.h"
#include "post.h"
#include "rpc.h"
#include "net.h"
#include "conf.h"
#include "fault.h"

#ifdef LOCK_LOG_VERBOSE
#define LOCK_LOG(lock, task, fmt, ...) do {\
  LOG("[%ld][core %d][lock %d][task %d] " fmt, timer_now(), \
    net_lcore_id(), lock, task, ##__VA_ARGS__); \
} while (0)
#else
#define LOCK_LOG(lock, task, fmt, ...) (0)
#endif

/* For skewed workloads, it is okay to disable local grants to avoid 
 * local mutex contention.
 */

// static bool enb_local_grants = false;
static bool enb_local_grants = true;

/* The metadata of in-network locks.
 *
 * For each lock, two maps are maintained to map (1) the lock key and
 * (2) the lock ID to the lock metadata structure. Note that the value
 * of the map is a pointer, so the lock metadata is not copied.
 */

typedef struct {
  mutex      mtx; /* A mutex lock for concurrent access to the lock metadata */

  // Basic metadata.
  lock_key   key;       /* The lock table key */
  lock_id    id;        /* The lock ID */
  lock_state mode;      /* FREE/LOCK_SHARED/LOCK_EXCL */
  bool       is_agent;  /* Whether the local machine is the lock agent */
  uint32_t   ncnt;      /* Received notification count */       

  // Only for the lock agent.
  unordered_set<lock_holder> holders; /* The lock holder set */
  vector<wqe>                wqueue;  /* The wait queue */

  // Conditional variable for local wait.
  unordered_map<task_id, atomic_bool> task_granted;
  condition_variable           wait_cv;

  // Variables for handling corner cases.
  bool delayed_free;
  unordered_map<lock_holder, bool> early_release;
  unordered_map<lock_holder, short> fwd_back_cnt;

  // Variables for failure recovery.
  unordered_set<task_id> local_holders;

} innet_lock;

/* The lock table.
 */

typedef innet_lock* innet_lock_ptr;
static unordered_map<lock_key, innet_lock_ptr> *lock_tbl;
static unordered_map<lock_id, innet_lock_ptr> *lock_id_map;

/* Register the packet preprocessor.
 */
static preprocessor_f packet_preprocess;
void register_packet_preprocessor(preprocessor_f f) {
  packet_preprocess = f;
}

/* Utility functions to get or update lock metadata.
 */
static inline lock_desc lock_id_to_desc(lock_id l) {
  auto inl = lock_id_map->find(l);
  if (inl == lock_id_map->end()) {
    innet_lock_ptr lock = new innet_lock();
    lock->id = l;
    lock->mode = FREE;
    lock->is_agent = false;
    lock->delayed_free = false;

    return (lock_desc)(lock_id_map->insert({l, lock}).first->second);
  }
  return (lock_desc)(inl->second);
}

static inline lock_desc lock_key_to_desc(lock_key l) {
  auto inl = lock_tbl->find(l);
  ASSERT_MSG(inl != lock_tbl->end(), "lock_key %lu", l);
  return (lock_desc)(inl->second);
}

static inline innet_lock* lmeta(lock_desc l) {
  return ((innet_lock *)l);
}

static inline lock_id id(lock_desc l) {
  return lmeta(l)->id;
}

static inline host_id lock_to_lm(lock_desc l) {
  return id(l) % HOST_NUM + 1;
}

static inline bool lock_out_of_range(lock_desc l) {
  return id(l) >= SWITCH_LOCK_NUM;
}

static inline void lock_add_holder(lock_desc l, lock_holder lh) {
  lmeta(l)->holders.insert(lh);
}

static inline void lock_remove_holder(lock_desc l, lock_holder lh) {
  lmeta(l)->holders.erase(lh);
}

static inline bool lock_has_holder(lock_desc l, lock_holder lh) {
  return lmeta(l)->holders.find(lh) != 
         lmeta(l)->holders.end();
}

static inline bool lock_no_holders(lock_desc l) {
  return lmeta(l)->holders.empty();
}

static inline void lock_append_to_wq(lock_desc l, wqe w) {
  lmeta(l)->wqueue.push_back(w);
}

static inline void lock_unmarshal_wq(lock_desc l, wqe* wq, size_t wq_sz) {
  int size = lmeta(l)->wqueue.size();
  lmeta(l)->wqueue.resize(wq_sz + size);
  net_memcpy(&lmeta(l)->wqueue[size], wq, wq_sz * sizeof(wqe));
}

static inline void lock_digest_wq(lock_desc lock, wqe* wq, size_t wq_sz, 
  task_id from_task) {
  uint64_t bhdls[DPDK_TX_BURST_SIZE];
  net_new_buf_bulk(bhdls, wq_sz);

  uint32_t grant_cnt = 0;
  for (int i = 0; i < wq_sz; i++) {

    // For shared requests, send them the grant reply and
    // add them to holders.
    if (wq[i].op == LOCK_SHARED) {
      timer_schedule_start(id(lock), from_task);
      lock_add_holder(lock, LOCK_HOLDER(wq[i].host, wq[i].task));

      char* buf = net_get_sendbuf(bhdls[grant_cnt++]);
      POST_TYPE(buf) = POST_LOCK_GRANT_WO_AGENT;
      lock_post_header* hdr = (lock_post_header*)POST_BODY(buf);
      hdr->mode_old_mode = 0;
      hdr->lock_id = htonl(id(lock));
      hdr->machine_id = wq[i].host;
      hdr->task_id = wq[i].task;
      hdr->agent = wq[i].host;
      hdr->wq_size = 0;
      hdr->ncnt = 0;
      timer_schedule_from_to(id(lock), from_task, wq[i].task);

    // For exclusive requests, append them to the wqueue.
    } else lock_append_to_wq(lock, wq[i]);
  }
 
  net_send_batch(0, bhdls, POST_HDR_SIZE(lock_post_header), grant_cnt);
}

static inline bool lock_no_waiters(lock_desc l) {
  return lmeta(l)->wqueue.empty();
}

static inline lock_req lock_prepare_wait(lock_desc l, task_id tid) {
  return (lock_req)(&(lmeta(l)->task_granted.emplace(
    tid, false).first->second));
}

static inline bool lock_local_wait(lock_desc l, task_id tid, 
                                   unique_lock<mutex> *lk) {


#if LOCK_WAIT_MECHANISM == LOCK_WAIT_BUSY_LOOP
  return lmeta(l)->task_granted[tid].load();
#else
  // If already notified, directly returns true.
  auto notified = &lmeta(l)->task_granted[tid];
  if (*notified) {
    *notified = false;
    return true;
  }

  // Otherwise, wait until notified or timeout.
  if (lmeta(l)->wait_cv.wait_for(
    *lk, 1000us, [notified](){ return *notified; })) {

    // If notified, reset the flag and returns true.
    *notified = false;
    return true;
  }

  // If timeout, returns false.
  return false;
#endif

}

static inline void lock_local_notify(lock_desc l, task_id tid) {
  lmeta(l)->local_holders.insert(tid);
#if LOCK_WAIT_MECHANISM == LOCK_WAIT_BUSY_LOOP
  lmeta(l)->task_granted[tid].store(true);
#else
  lmeta(l)->wait_cv.notify_all();
#endif
}

/* The lock agent handlers.
 * 
 * Note that the lock metadata must be locked before 
 * calling any handler.
 */
static void handle_granted_acquire(lock_desc lock, host_id hid, task_id tid) {
  LOCK_LOG(id(lock), tid, "granted request from host %u", hid);
  lock_add_holder(lock, LOCK_HOLDER(hid, tid));
}

static void handle_suspend_acquire(lock_desc lock, host_id hid, task_id tid, 
                                   lock_state op) {
  LOCK_LOG(id(lock), tid, "suspended request from host %u", hid);
  ASSERT_MSG(op != FREE, "lock %u", id(lock));
  wqe req = {.op = op, .task = tid, .host = hid};
  lock_append_to_wq(lock, req);
}

static void handle_release(lock_desc lock, host_id hid, task_id tid,
                           unique_lock<mutex> *lk) {
  LOCK_LOG(id(lock), tid, "handling release request from host %u", hid);
  ASSERT_MSG(lmeta(lock)->mode != FREE, "releasing a free lock %u", id(lock));
  lock_remove_holder(lock, LOCK_HOLDER(hid, tid));
}

static bool handle_acquire_lm(lock_desc lock, host_id hid, task_id tid,
                              lock_state op) {

  // FREE <- any
  if (lmeta(lock)->mode == FREE) {
    lmeta(lock)->mode = op;
    lock_add_holder(lock, LOCK_HOLDER(hid, tid));
    return true;

  // SHARED <- SHARED
  } else if (lmeta(lock)->mode == LOCK_SHARED) {
    if (op == LOCK_SHARED) {
      lock_add_holder(lock, LOCK_HOLDER(hid, tid));
      return true;

    // SHARED <- EXCL
    } else {
      wqe e = {.op = op, .task = tid, .host = hid};
      lock_append_to_wq(lock, e);
      return false;
    }

  // EXCL <- any
  } else {
    wqe e = {.op = op, .task = tid, .host = hid};
    lock_append_to_wq(lock, e);
    return false;
  }
}

/* Return true: send reply; false: drop packet
 */
static bool handle_release_lm(lock_desc lock, host_id hid, task_id tid) {
  lock_remove_holder(lock, LOCK_HOLDER(hid, tid));
  LOCK_LOG(id(lock), tid, "released out-of-range request");

  // Free
  if (lock_no_holders(lock)) {
    if (lock_no_waiters(lock)) {
      LOCK_LOG(id(lock), tid, "lock becomes free");
      lmeta(lock)->mode = FREE;

  // Schedule
    } else {
      LOCK_LOG(id(lock), tid, "scheduling to the next");
      lmeta(lock)->mode = (lock_state)lmeta(lock)->wqueue[0].op;

      if (lmeta(lock)->mode == LOCK_SHARED) {
        vector<wqe> wq = lmeta(lock)->wqueue;
        lmeta(lock)->wqueue.clear();
        lock_digest_wq(lock, &wq[0], wq.size(), tid);

      } else {
        return true;
      }
    }
  }

  return false;
}

// Change the current packet into a free packet.
static inline void lock_free_helper(lock_desc lock, post_header* ph, 
                                   lock_post_header* hdr) {
  LOCK_LOG(id(lock), hdr->task_id, "freed with ncnt %u", lmeta(lock)->ncnt);

  ph->type = POST_LOCK_FREE;
  hdr->mode_old_mode = MODE_OLD_MODE(0, lmeta(lock)->mode);
  hdr->machine_id = LOCALHOST_ID;
  hdr->task_id = 0;
  hdr->agent = LOCALHOST_ID;
  hdr->wq_size = 0;
  hdr->ncnt = (char)(lmeta(lock)->ncnt);

  lmeta(lock)->is_agent = false;
  lmeta(lock)->mode = FREE;
  lmeta(lock)->ncnt = 0;
}

// Change the current packet into a transfer packet.
static inline int lock_transfer_helper(lock_desc lock, post_header* ph, 
                                   lock_post_header* hdr) {
  auto wq_entry = lmeta(lock)->wqueue.begin();
  auto wq_size = lmeta(lock)->wqueue.size() - 1;
  auto wq_addr = (void *)&lmeta(lock)->wqueue[1];

  auto from_task = hdr->task_id;
  timer_schedule_start(id(lock), from_task);

  LOCK_LOG(id(lock), hdr->task_id, 
    "scheduled to task %u host %u with ncnt %u, wqsize %lu", 
    wq_entry->task, wq_entry->host, lmeta(lock)->ncnt,
    lmeta(lock)->wqueue.size());

  ph->type = POST_LOCK_TRANSFER;
  hdr->mode_old_mode = MODE_OLD_MODE(wq_entry->op, lmeta(lock)->mode);
  hdr->machine_id = wq_entry->host;
  hdr->task_id = wq_entry->task;
  hdr->agent = LOCALHOST_ID;
  hdr->wq_size = wq_size;
  hdr->ncnt = (char)(lmeta(lock)->ncnt);

  if (wq_size > 0) {
    net_memcpy((void *)(hdr + 1), wq_addr, wq_size * sizeof(wqe));
  }

  lmeta(lock)->is_agent = false;
  lmeta(lock)->ncnt = 0;
  lmeta(lock)->mode = FREE;
  lmeta(lock)->wqueue.clear();

  // timer_since_burst_schedule_start(id(lock), hdr->task_id);
  timer_schedule_from_to(id(lock), from_task, wq_entry->task);
  return wq_size;
}

/* The processor of lock packets.
 * Returns 0 in normal cases;
 * returns 1 if the packet need to be forwarded back to the switch.
 */
int lock_packet_process(post_header* ph, lock_desc lock, 
  lock_post_header* hdr) {

  // Execute the application-defined preprocessor (if exist).
  if (packet_preprocess != NULL)
    packet_preprocess((post_t)(ph->type), hdr);

  // Process the packet based on the type.
  switch (ph->type) {

    // We are the agent of a lock, and the switch delegates
    // the lock request to us.
    case POST_LOCK_ACQUIRE:
    {
      unique_lock<mutex> lk(lmeta(lock)->mtx);
      LOCK_LOG(id(lock), hdr->task_id, "received acquire request from host %u",
        hdr->machine_id);

      // Server-handled locks.
      if (unlikely(lock_out_of_range(lock))) {
        bool grant = handle_acquire_lm(lock, hdr->machine_id, hdr->task_id, 
          MODE(hdr->mode_old_mode));
        lk.unlock();
        LOCK_LOG(id(lock), hdr->task_id, "%s out-of-range request",
          grant ? "granted" : "suspended");

        // If granted, send reply.
        if (grant) {
          ph->type = POST_LOCK_GRANT_WO_AGENT;
          hdr->agent = hdr->machine_id; /* agent is dest here */
          return 1;
        }

        return 0;
      }

      // Shared acquire packets may come to us before we're granted the agent
      // in two occasions:
      // 
      // 1. it is a request that arrives at the switch later than our acquire
      //    but arrives at the server sooner than our grant reply. If our grant
      //    reply is exclusive, ncnt is useless; otherwise, need to count it.
      // 2. the local host becomes the agent after we sharedly acquire 
      //    the lock, and becomes free before the request is forwarded 
      //    back to local host. If the agent is shared, the free will not
      //    succeed because the ncnt does not match; if the agent is exclusive,
      //    we do not need to count it.
      if (MODE(hdr->mode_old_mode) == LOCK_SHARED && GRANTED(hdr)) {
        lmeta(lock)->ncnt++;
        LOCK_LOG(id(lock), hdr->task_id, "ncnt++, now %u", lmeta(lock)->ncnt);
      }

      // If the release request from the client arrives earlier,
      // we directly perform the delayed release here.
      lock_holder lh = LOCK_HOLDER(hdr->machine_id, hdr->task_id);
      if (lmeta(lock)->early_release[lh]) {
        lmeta(lock)->early_release[lh] = false;
        lk.unlock();
        return 0;
      }

      // This happens when we unlocked before the lock request arrives
      // or the grant with agent packet is delayed.
      if (!lmeta(lock)->is_agent) {

        // If the lock is in shared state before we freed, and the incoming
        // request is also shared, the free is bound to fail. Hence, we just
        // record the new holder.
        if (MODE(hdr->mode_old_mode) == LOCK_SHARED) {
          if (GRANTED(hdr)) {
            handle_granted_acquire(lock, hdr->machine_id, hdr->task_id);
            lk.unlock();
            return 0;
          }

        // If the same exclusive request has been forwarded back for a few
        // times, there should be a delayed grant with agent packet. So we
        // append the request to the wqueue ahead-of-time.
        } else {
          lock_holder lh = LOCK_HOLDER(hdr->machine_id, hdr->task_id);
          if (lmeta(lock)->fwd_back_cnt[lh] > 2) {
            handle_suspend_acquire(lock, hdr->machine_id, 
              hdr->task_id, MODE(hdr->mode_old_mode));
            lmeta(lock)->fwd_back_cnt.erase(lh);
            return 0;

          // Otherwise, increment the fwd_back counter and forward back.
          } else lmeta(lock)->fwd_back_cnt[lh]++;
        }

        // Otherwise, the agent should has been transferred to other machines. 
        // Hence, we forward this lock request back to the decider.
        lk.unlock();
        LOCK_LOG(id(lock), hdr->task_id, "request forwarded back");
        count_fwd_back();
        return 1;
     
      // Otherwise, handle the request according to the lock mode.
      } else {
        if (GRANTED(hdr)) {
          handle_granted_acquire(lock, hdr->machine_id, hdr->task_id);
          lk.unlock();

        } else {
          handle_suspend_acquire(lock, hdr->machine_id, 
            hdr->task_id, MODE(hdr->mode_old_mode));
          lk.unlock();
        }
      }

      return 0;
    }

    // The switch grant the lock and the agent to us.
    case POST_LOCK_GRANT_WITH_AGENT:
    {
      // Do not process packets for failure recovery.
      if (hdr->task_id == TASK_FOR_RESTORE) return 0;

      timer_grant_begin(id(lock), hdr->task_id);
      unique_lock<mutex> lk(lmeta(lock)->mtx);

      // Update local lock metadata.
      ASSERT_MSG(!lmeta(lock)->is_agent, 
        "host %u lock %d task %d redundant agent",
        LOCALHOST_ID, id(lock), hdr->task_id);
      ASSERT(hdr->agent == LOCALHOST_ID);
      lmeta(lock)->is_agent = true;
      lock_add_holder(lock, LOCK_HOLDER(LOCALHOST_ID, hdr->task_id));

      // This can be a switch grant or a transfer.
      // If the op is FREE, then this is a transfer. In this case,
      // we update the mode to next_op.
      // Ncnt is resetted when acquire for the lock, see comments for the
      // acquire function to find the reason.
      if (TRANSFERRED(hdr)) {

        LOCK_LOG(id(lock), hdr->task_id, "received transfer packet "
          "from host %u, wqsize %u", hdr->machine_id, hdr->wq_size);
        lmeta(lock)->mode = MODE(hdr->mode_old_mode);
        lmeta(lock)->ncnt += 0;

        // If the wait queue is also transferred, unmarshal it.
        if (hdr->wq_size > 0) {
          wqe* wq = (wqe *)(hdr + 1);

          // If the current holder is shared, also grant the lock to
          // shared requests in the wqueue.
          if (lmeta(lock)->mode == LOCK_SHARED) {
            lock_digest_wq(lock, wq, hdr->wq_size, hdr->task_id);

          // Otherwise, insert the whole wqueue in the packet into
          // the wqueue in the lock table.
          } else lock_unmarshal_wq(lock, wq, hdr->wq_size);
        }

      // Otherwise, the grant is sent by switch.
      } else {
        LOCK_LOG(id(lock), hdr->task_id, "received grant with agent packet");
        lmeta(lock)->mode = MODE(hdr->mode_old_mode);

        // All shared acquire packets increments the on-switch ncounter,
        // so we need to increment the local counter too. We do not reset
        // here, but reset when acquire for the lock, see comments for the
        // acquire function to find the reason.
        if (lmeta(lock)->mode == LOCK_SHARED) {
          lmeta(lock)->ncnt++;
        } else lmeta(lock)->ncnt = 0;
      }

      LOCK_LOG(id(lock), hdr->task_id, "notifying the app thread");
      lock_local_notify(lock, hdr->task_id);
      lk.unlock();

      timer_grant_w_agent(id(lock), hdr->task_id);
      count_grant_with_agent();
      return 0;
    }

    // The switch grant the lock but not the agent to us.
    case POST_LOCK_GRANT_WO_AGENT:
    {
      timer_grant_begin(id(lock), hdr->task_id);
      ASSERT(hdr->machine_id == LOCALHOST_ID);
      LOCK_LOG(id(lock), hdr->task_id, "received grant without agent packet");

      // If we are the agent, it should be this case:
      // we freed the lock -> the next acquire is sent to switch ->
      // the free failed and we continue to be the agent ->
      // the switch granted the lock and replied us.
      // 
      // In this case, we will receive a request and a reply.
      // Since all operations here are idempotent, nothing need
      // to be done.
      // if (lmeta(lock)->is_agent) ;

      unique_lock<mutex> lk(lmeta(lock)->mtx);
      lock_local_notify(lock, hdr->task_id);
      lk.unlock();

      timer_grant_wo_agent(id(lock), hdr->task_id);
      count_grant_wo_agent();
      return 0;
    }

    // Handle release requests as the agent.
    case POST_LOCK_RELEASE:
    {
      unique_lock<mutex> lk(lmeta(lock)->mtx);
      LOCK_LOG(id(lock), hdr->task_id, "received release packet from host %u",
        hdr->machine_id);

      // Server-handled locks.
      if (unlikely(lock_out_of_range(lock))) {
        if (handle_release_lm(lock, hdr->machine_id, hdr->task_id)) {
          auto from_task = hdr->task_id;
          timer_schedule_start(id(lock), from_task);
          auto wqe = lmeta(lock)->wqueue.begin();

          ph->type = POST_LOCK_GRANT_WO_AGENT;
          hdr->machine_id = wqe->host;
          hdr->task_id = wqe->task;
          hdr->agent = wqe->host;
          timer_schedule_from_to(id(lock), from_task, wqe->task);
          lmeta(lock)->wqueue.erase(wqe);
          lk.unlock();
          return 1;
        }

        lk.unlock();
        return 0;
      }

      // If the holder is not present, we first try to find the holder
      // in the wait queue, as it might be released by aborted transactions.
      if (!lock_has_holder(lock, LOCK_HOLDER(hdr->machine_id, hdr->task_id))) {
        auto wq = &lmeta(lock)->wqueue;
        for (auto i = wq->begin(); i != wq->end(); i++) {
          if (i->host == hdr->machine_id && i->task == hdr->task_id) {
            wq->erase(i);
            lk.unlock();
            return 0;
          }
        }

        // If not found, the acquire request should be delayed.
        // Since the release indicates that the lock is already granted, we
        // perform release right away and record that the lock has been released
        // to avoid double-acquire.
        lock_holder lh = LOCK_HOLDER(hdr->machine_id, hdr->task_id);
        lmeta(lock)->early_release[lh] = true;
        LOCK_LOG(id(lock), hdr->task_id, "released before delayed acquire");

      // If we are not the agent while there are remaining holders, 
      // it means the previous free failed but is not received by us.
      // In this case, we just remove the holder and continue to be the agent.
      } else if (!lmeta(lock)->is_agent) {
        lock_remove_holder(lock, LOCK_HOLDER(hdr->machine_id, hdr->task_id));
        lmeta(lock)->delayed_free = true;
        lk.unlock();

        LOCK_LOG(id(lock), hdr->task_id, "released before delayed free");
        return 0;

      // Otherwise, handle the release request. 
      } else {
        handle_release(lock, hdr->machine_id, hdr->task_id, &lk);
      }

      // If all current holders have released the lock, free
      // or transfer the lock.
      if (lock_no_holders(lock)) {
        if (lock_no_waiters(lock)) { /* Lock free */
          lock_free_helper(lock, ph, hdr);
          lk.unlock();
          return 1;

        } else { /* Lock transfer */
          int wq_size = lock_transfer_helper(lock, ph, hdr);
          lk.unlock();
          return wq_size ? (wq_size * sizeof(wqe)) : 1;
        }
      } 
 
      LOCK_LOG(id(lock), hdr->task_id, "%lu holders remain after release", 
        lmeta(lock)->holders.size());
      lk.unlock();
      return 0;
    }

    // The switch pushes back the transfer/free request we sent,
    // because the notification count does not match.
    // In this case, we continue to be the agent.
    case POST_LOCK_TRANSFER:
    case POST_LOCK_FREE:
    {
      unique_lock<mutex> lk(lmeta(lock)->mtx);
      LOCK_LOG(id(lock), hdr->task_id, "task transfer or free failed");

      // If the request that causes the free to fail has been handled,
      // try to free or transfer again with updated ncnt.
      if (lmeta(lock)->delayed_free && lock_no_holders(lock)) {
        hdr->ncnt += lmeta(lock)->ncnt;
        lmeta(lock)->ncnt = 0;
        lmeta(lock)->delayed_free = false;

        LOCK_LOG(id(lock), hdr->task_id, "try to free again");
        return 1;
      }

      LOCK_LOG(id(lock), 0, "ncnt %u -> %u",
        lmeta(lock)->ncnt, lmeta(lock)->ncnt + hdr->ncnt);
      ASSERT_MSG(lmeta(lock)->mode == FREE, "host %d lock %d mode not free",
        LOCALHOST_ID, id(lock));
      lmeta(lock)->mode = OLD_MODE(hdr->mode_old_mode);
      lmeta(lock)->is_agent = true;
      lmeta(lock)->ncnt += hdr->ncnt;

      // If this is a transfer, restore the wait queue.
      if (ph->type == POST_LOCK_TRANSFER) {
        LOCK_LOG(id(lock), hdr->task_id, "restore the wqueue");
        wqe e = {.op = MODE(hdr->mode_old_mode), .task = hdr->task_id, 
          .host = hdr->machine_id};
        lmeta(lock)->wqueue.push_back(e);

        if (hdr->wq_size > 0) {
          wqe* wq = (wqe *)(hdr + 1);
          lmeta(lock)->wqueue.insert(lmeta(lock)->wqueue.end(), 
            wq, wq + hdr->wq_size);
        }
      }

      lk.unlock();
      return 0;
    }
  }

  return 0;
}

/* The dispatcher for lock packets.
 */
int lock_packet_dispatch(void* buf, uint32_t core) {
  lock_post_header* hdr = (lock_post_header *)POST_BODY(buf);
  lock_desc lock = lock_id_to_desc(ntohl(hdr->lock_id));
  return lock_packet_process((post_header *)buf, lock, hdr);
}

/* Set up the lock module.
 *
 * Should be called before the application starts.
 */
int lock_setup(uint32_t flags) {
  enb_local_grants = (flags & FLAG_ENB_LOCAL_GRANT) != 0;
  lock_tbl = new unordered_map<lock_key, innet_lock_ptr>;
  lock_id_map = new unordered_map<lock_id, innet_lock_ptr>;
  register_packet_dispatcher(POST_LOCK_ACQUIRE, lock_packet_dispatch);
  register_packet_dispatcher(POST_LOCK_GRANT_WITH_AGENT, lock_packet_dispatch);
  register_packet_dispatcher(POST_LOCK_GRANT_WO_AGENT, lock_packet_dispatch);
  register_packet_dispatcher(POST_LOCK_RELEASE, lock_packet_dispatch);
  register_packet_dispatcher(POST_LOCK_TRANSFER, lock_packet_dispatch);
  register_packet_dispatcher(POST_LOCK_FREE, lock_packet_dispatch);
  return 0;
}

/* Register the lock to the manager.
 * 
 * The argument can be any key that is not bigger than 64 bits.
 * This function assigns an ID to the lock.
 */
lock_id lock_init(lock_key lkey) {
  const char* req_buf = (const char*)(&lkey);

  // If the lock has been inited, just return.
  if (lock_tbl->find(lkey) != lock_tbl->end()) {
    return 0;
  }

  // Register the lock at the switch and get lock id.
  lock_id* lid_ptr = (lock_id *)rpc(RPC_LOCK_REGISTER, req_buf, 8, NULL);

  // Allocate the lock metadata structure. 
  innet_lock_ptr l = new innet_lock();
  l->key = lkey;
  l->id = *lid_ptr;
  l->mode = FREE;
  l->is_agent = false;
  l->delayed_free = false;

  // Record the lock in the maps.
  lock_tbl->insert({lkey, l});
  lock_id_map->insert({l->id, l});

  return l->id;
}

/* Init the lock structure locally.
 * 
 * Should only be used in benchmarks.
 */
lock_id lock_local_init(uint64_t lock_id) {

  // If the lock has been inited, just return.
  if (lock_tbl->find(lock_id) != lock_tbl->end()) {
    return 0;
  }

  // Allocate the lock metadata structure. 
  innet_lock_ptr l = new innet_lock();
  l->key = lock_id;
  l->id = lock_id;
  l->mode = FREE;
  l->is_agent = false;
  l->delayed_free = false;

  // Record the lock in the maps.
  lock_tbl->insert({lock_id, l});
  lock_id_map->insert({l->id, l});

  return l->id;
}

/* Acquire the lock, non-blocking.
 *
 * Returns 0 if the lock is granted immediately, 1 if the lock is
 * suspended locally, 2 if the lock is waiting for a remote reply,
 * 3 if the lock has a wait timeout.
 */
lock_req lock_acquire_async(uint64_t lkey, task_id task, lock_state op) {
  if (op != LOCK_EXCL && op != LOCK_SHARED) return -ERR_INVALID_OP;
  lock_desc lock = lock_key_to_desc(lkey);
  lock_req req;

#ifdef FISSLOCK_FAILURE_RECOVERY
  check_error(CALLER_ACQUIRE_OR_RELEASE, lmeta(lock)->id, task);
#endif

  timer_acquire(id(lock), task);

#ifdef TPCC_ENB_LOCAL

  // In TPCC workload, only customer and item tables may be accessed
  // by remote clients, so all the other locks can be processed purely
  // locally.
  auto lid = id(lock) % TPCC_WAREHOUSE_LOCK_NUM;
  if (!((lid >= 11 && lid < 41) || /* Customer */
        (lid >= 111 && lid < 121)  /* Item */
  )) {
    timer_acquire_sent(id(lock), task);

    unique_lock<mutex> lk(lmeta(lock)->mtx);
    if (handle_acquire_lm(lock, LOCALHOST_ID, task, op)) {
      lk.unlock();
      timer_grant_begin(id(lock), task);
      timer_grant_local(id(lock), task);
      return 0;
    } else {
      req = lock_prepare_wait(lock, task);
      lk.unlock();
      return req;
    }
  }
#endif

  // For locks not managed by switch, fall back to parlock.
  if (lock_out_of_range(lock)) {
    LOCK_LOG(id(lock), task, "out-of-range %s acquire",
      (op == LOCK_SHARED) ? "shared" : "exclusive");

    unique_lock<mutex> lk(lmeta(lock)->mtx);
    if (lock_to_lm(lock) == LOCALHOST_ID) {
      timer_acquire_sent(id(lock), task);
      if (handle_acquire_lm(lock, LOCALHOST_ID, task, op)) {
        lk.unlock();
        timer_grant_begin(id(lock), task);
        timer_grant_local(id(lock), task);
        return 0;

      } else {
        req = lock_prepare_wait(lock, task);
        lk.unlock();
        return req;
      }
    }

    req = lock_prepare_wait(lock, task);
    lk.unlock();
    send_post_acquire_lm(op, id(lock), lock_to_lm(lock), task);
    timer_acquire_sent(id(lock), task);
    return req;
  }

  // If local granting is enabled, we acquire the mutex to protect
  // subsequent accesses to the lock metadata.
  if (enb_local_grants) {
    unique_lock<mutex> lk(lmeta(lock)->mtx);

    // Fast path: if the local host is the agent, handle the 
    // request locally.
    if (lmeta(lock)->is_agent) {
      LOCK_LOG(id(lock), task, "local %s acquire",
        (op == LOCK_SHARED) ? "shared" : "exclusive");
      timer_acquire_sent(id(lock), task);

      // For shared lock and shared acquire, grant the lock right away.
      if (lmeta(lock)->mode == LOCK_SHARED && op == LOCK_SHARED) {
        handle_granted_acquire(lock, LOCALHOST_ID, task);
        lk.unlock();

        timer_grant_begin(id(lock), task);
        timer_grant_local(id(lock), task);
        return 0;

      // For exclusive lock, straightly append to the wait queue.
      // In this case, the grant certainly comes later from the network.
      } else {
        handle_suspend_acquire(lock, LOCALHOST_ID, task, op);
        req = lock_prepare_wait(lock, task);
        lk.unlock();
        return req;
      }

    // Slow path: mark the requester, prepare to send the lock request.
    } else {
      req = lock_prepare_wait(lock, task);
      lk.unlock();
    }
  }

  // send a lock request to the switch.
  LOCK_LOG(id(lock), task, "remote %s acquire",
    (op == LOCK_SHARED) ? "shared" : "exclusive");
  send_post_acquire(op, id(lock), LOCALHOST_ID, task);
  timer_acquire_sent(id(lock), task);

  return req;
}

/* Wait until the lock is granted by a local notification.
 *
 * Note that this function can be timeout, in which case it returns 1.
 * In normal cases, the function returns 0.
 */
int lock_wait_local(uint64_t lkey, task_id task) {
  lock_desc lock = lock_key_to_desc(lkey);

  // Wait for local grant.
  unique_lock<mutex> lk(lmeta(lock)->mtx);
  auto ret = lock_local_wait(lock, task, &lk);
  lk.unlock();

  return !ret;
}

int lock_req_granted(lock_req req, lock_id lock, task_id task) {
  if (!req) return 1;

#ifdef FISSLOCK_FAILURE_RECOVERY
  if (check_error(CALLER_WAIT_FOR_GRANT, lock, task)) return 2;
#endif

  if (((atomic_bool *)req)->load()) {
    ((atomic_bool *)req)->store(false);
    return 1;
  } else return 0;
}

/* Acquire the lock, blocking.
 */
int lock_acquire(uint64_t lkey, task_id task, lock_state op) {
  int ret = lock_acquire_async(lkey, task, op);
  switch (ret) {
    case 0: return 0;
    case 1: case 2: return lock_wait_local(lkey, task);
    case 3: return 1;
  }
  return 0;
}

/* Release the lock, non-blocking.
 * Returns 0 if the release succeeded, 1 otherwise.
 */
int lock_release(uint64_t lkey, task_id task, lock_state op) {
  lock_desc lock = lock_key_to_desc(lkey);

#ifdef FISSLOCK_FAILURE_RECOVERY
  check_error(CALLER_ACQUIRE_OR_RELEASE, lmeta(lock)->id, task);
#endif

  // If we are the agent, handle the request locally.
  timer_release(id(lock), task);
  unique_lock<mutex> lk(lmeta(lock)->mtx);

  lmeta(lock)->local_holders.erase(task);

#ifdef TPCC_ENB_LOCAL

  // In TPCC workload, only customer and item tables may be accessed
  // by remote clients, so all the other locks can be processed purely
  // locally.
  auto lid = id(lock) % TPCC_WAREHOUSE_LOCK_NUM;
  if (!((lid >= 11 && lid < 41) || /* Customer */
        (lid >= 111 && lid < 121)  /* Item */
  )) {
    timer_release_sent(id(lock), task);
    timer_schedule_start(id(lock), task);

    if (handle_release_lm(lock, LOCALHOST_ID, task)) {
      auto wqe = lmeta(lock)->wqueue.begin();
      ASSERT(wqe->host == LOCALHOST_ID);
      lock_local_notify(lock, wqe->task);
      timer_schedule_from_to(id(lock), task, wqe->task);
      timer_grant_w_agent(id(lock), wqe->task);
      lmeta(lock)->wqueue.erase(wqe);
    }

    lk.unlock();
    return 0;
  }
#endif

  // Server-handled locks.
  if (unlikely(lock_out_of_range(lock))) {
    LOCK_LOG(id(lock), task, "out-of-range release");

    if (lock_to_lm(lock) == LOCALHOST_ID) {
      timer_release_sent(id(lock), task);

      if (handle_release_lm(lock, LOCALHOST_ID, task)) {
        timer_schedule_start(id(lock), task);
        auto wqe = lmeta(lock)->wqueue.begin();
        send_post_grant(id(lock), wqe->host, wqe->task);
        timer_schedule_from_to(id(lock), task, wqe->task);
        lmeta(lock)->wqueue.erase(wqe);
      }

      lk.unlock();
      return 0;
    }

    lk.unlock();
    send_post_release_lm(id(lock), LOCALHOST_ID, task, lock_to_lm(lock));
    timer_release_sent(id(lock), task);
    return 0;
  }

  if (lmeta(lock)->is_agent) {
    LOCK_LOG(id(lock), task, "local release");

    // If we sent an shared acquire packet and subsequently becomes the 
    // agent, we might receive the grant before receiving the notification.
    // Hence, we wait until the lock has holder here.
    lock_holder lh = LOCK_HOLDER(LOCALHOST_ID, task);
    if (!lock_has_holder(lock, lh)) {
      lmeta(lock)->early_release[lh] = true;
      LOCK_LOG(id(lock), task, "released before delayed acquire");

    } else handle_release(lock, LOCALHOST_ID, task, &lk);

    // If there are no current holders, transfer of free the lock.
    timer_release_local(id(lock), task);
    if (lock_no_holders(lock)) {
      if (lock_no_waiters(lock)) { /* Lock free */
        LOCK_LOG(id(lock), task, "free the lock");
        char ncnt = (char)(lmeta(lock)->ncnt);
        lock_state old_state = lmeta(lock)->mode;
        lmeta(lock)->is_agent = false;
        lmeta(lock)->mode = FREE;
        lmeta(lock)->ncnt = 0;
        lk.unlock();
  
        send_post_free(id(lock), ncnt, old_state);
        return 0;

      } else { /* Lock transfer */
        LOCK_LOG(id(lock), task, "transfer the lock");
        timer_schedule_start(id(lock), task);
        auto wq_entry = lmeta(lock)->wqueue.begin();
        char ncnt = (char)(lmeta(lock)->ncnt);
        lock_state old_state = lmeta(lock)->mode;
        lmeta(lock)->is_agent = false;
        lmeta(lock)->mode = FREE;
        lmeta(lock)->ncnt = 0;
        timer_schedule_from_to(id(lock), task, wq_entry->task);

        ASSERT_MSG(lmeta(lock)->wqueue.size() * sizeof(wqe) < 2000,
          "wait queue is too long, can not transfer");

        send_post_transfer((lock_state)wq_entry->op, id(lock), 
          wq_entry->host, wq_entry->task, 
          lmeta(lock)->wqueue.size() - 1, (wqe *)&lmeta(lock)->wqueue[1],
          ncnt, old_state);

        lmeta(lock)->wqueue.clear();
        lk.unlock();
        return 0;
      }
    } 

    lk.unlock();

  // Otherwise, just send a release request.
  } else {
    LOCK_LOG(id(lock), task, "remote release");
    lk.unlock();

    send_post_release(id(lock), LOCALHOST_ID, task);
    timer_release_sent(id(lock), task);
  }

  return 0;
}

void integrity_check(uint64_t fault_code) {
  steady_clock::time_point tp = steady_clock::now();
  vector<innet_lock_ptr> lks_w_agt;

  // Scan the lock table: 
  for (auto lte : *lock_tbl) {
    auto lock = lte.second;

    // Collect the local lock holders.
    if (lock->local_holders.size()) {
      LOG("collect %ld local holders of lock %d",
        lock->local_holders.size(), lock->id);

      for (auto holder: lock->local_holders) {
        collect_holders_req req = {.lid = lock->id,
          .holder = LOCK_HOLDER(LOCALHOST_ID, holder)};
        rpc(RPC_COLLECT_HOLDERS, (char *)&req, sizeof(req), NULL);
      }
    }

    // Remove holders and waiters on the failed machine.
    if (lock->is_agent) {
      for (int i = 0; i < HOST_NUM; i++) {
        if (!SERVER_FAILED(fault_code, i)) continue;

        for (auto holder : lock->holders)
          if (LOCK_HOLDER_HOST(holder) == i)
            lock->holders.erase(holder);

        for (auto w = lock->wqueue.begin(); w < lock->wqueue.end(); w++)
          if (w->host == i) lock->wqueue.erase(w);
      }

      // Collect waiters of the lock.
      if (lock->wqueue.size() > 0) {
        LOG("collect %ld waiters of lock %d", lock->wqueue.size(), lock->id);

        for (auto waiter: lock->wqueue) {
          collect_waiters_req req = {.lid = lock->id,
            .waiter = LOCK_HOLDER(waiter.host, waiter.task)};
          rpc(RPC_COLLECT_WAITERS, (char *)&req, sizeof(req), NULL);
        }
      }
    }
  }

  LOG("submitted local holders and waiters to the coordinator, "
      "time consumption: %ld us", duration_cast<microseconds>(
        steady_clock::now() - tp).count());
  tp = steady_clock::now();

  // Tell the RPC daemon that we're ready, and wait until other
  // machines and the switch are also ready.
  recovery_ready_req req = {.mid = LOCALHOST_ID, .fault_code = fault_code};
  recovery_ready_reply* reply = NULL;

  while (!reply || reply->ready == 0) {
    sleep(1);
    reply = (recovery_ready_reply *)rpc(RPC_RECOVERY_READY, 
      (char *)&req, sizeof(req), NULL);
  }

  LOG("all machines have submitted their metas and the switch is ready.");
  tp = steady_clock::now();

  for (auto lte : *lock_tbl) {
    auto lock = lte.second;

    // For each managed lock, check whether there are holders not recorded. 
    if (lock->is_agent) {
      check_holder_req req = {.is_agent = true, .lid = lock->id, 
        .holder_num = (uint32_t)lock->holders.size()};

      // Provide the total amount of recorded holders to the RPC daemon.
      // If it is smaller than the actual number of holders, the reply
      // would contain all holders.
      auto reply = (check_holder_reply *)rpc(RPC_CHECK_HOLDER, 
        (char *)&req, sizeof(req), NULL);

      int32_t holder_cnt = reply->extra_num;
      if (holder_cnt > 0) {
        LOG("adding missed %ld holders", holder_cnt - lock->holders.size());
        for (int i = 0; i < holder_cnt; i++)
          lock->holders.insert(reply->extra_holders[i]);
      }
      lks_w_agt.push_back(lock);

    // For each unmanaged lock, check whether we need to become the agent.
    } else {
      if (lock->local_holders.size() > 0) {

        check_holder_req req = {.is_agent = false, .lid = lock->id};
        auto reply = (check_holder_reply *)rpc(RPC_CHECK_HOLDER, 
          (char *)&req, sizeof(req), NULL);

        int32_t holder_cnt = reply->extra_num;
        if (holder_cnt > 0) {
          LOG("restoring agent of lock %d", lock->id);
          lock->is_agent = true;
          lock->mode = (holder_cnt > 1) ? LOCK_SHARED : LOCK_EXCL;
          for (int i = 0; i < holder_cnt; i++)
            lock->holders.insert(reply->extra_holders[i]);
          lks_w_agt.push_back(lock);
        }
      }
    }
  }

  LOG("fixed/restored corrupted/missed lock metadata, "
      "time consumption: %ld us", duration_cast<microseconds>(
        steady_clock::now() - tp).count());
  tp = steady_clock::now();

  // We re-start the switch to flush outdated mid, and restore the mid and 
  // mode by sending an acquire packet from each agent.
  // if (fault_code & 0x1) {
  for (auto lock : lks_w_agt) {
    send_post_acquire(lock->mode, lock->id, LOCALHOST_ID, TASK_FOR_RESTORE);
  }

  LOG("restored switch states, agent number %ld, "
      "time consumption: %ld us", lks_w_agt.size(),
      duration_cast<microseconds>(steady_clock::now() - tp).count());
  tp = steady_clock::now();
}