
#include <sys/mman.h>

#include <unordered_map>
#include <set>
#include <cstring>

#include "core/lib.hh"
#include "core/rctrl.hh"

#include "rpc.h"
#include "debug.h"

using namespace rdmaio;
using std::unordered_map;
using std::set;

typedef struct {
  bool has_agent;
  set<uint64_t> holders;
  set<uint64_t> waiters;
} lock_data;

unordered_map<uint32_t, lock_data> lm_state;
static uint64_t fault_signal;
static uint8_t* ready_signal;
static uint64_t ready_bitmap;

/* An RPC server implemented by mimicing rib's RCtrl.
 */
class rpc_server {

  // The RPC server states and metadata.
  std::atomic<bool> running;
  pthread_t handler_tid;
  bootstrap::SRpcHandler rpc;

  // The lock table.
  std::unordered_map<uint64_t, lock_id> lock_tbl;
  lock_id lid_pool;

public:
  explicit rpc_server(const usize &port)
      : running(false), rpc(port, RPC_SERVER_IP), lid_pool(0) {

    // Register all rpc handlers.
    RDMA_ASSERT(
      rpc.register_handler(RPC_LOCK_REGISTER,
        std::bind(&rpc_server::lock_register, this, std::placeholders::_1))
    );

    RDMA_ASSERT(
      rpc.register_handler(RPC_RECOVERY_START,
        std::bind(&rpc_server::recovery_start, this, std::placeholders::_1))
    );

    RDMA_ASSERT(
      rpc.register_handler(RPC_COLLECT_HOLDERS,
        std::bind(&rpc_server::collect_holders, this, std::placeholders::_1))
    );

    RDMA_ASSERT(
      rpc.register_handler(RPC_COLLECT_WAITERS,
        std::bind(&rpc_server::collect_waiters, this, std::placeholders::_1))
    );

    RDMA_ASSERT(
      rpc.register_handler(RPC_CHECK_WAITER,
        std::bind(&rpc_server::check_waiter, this, std::placeholders::_1))
    );

    RDMA_ASSERT(
      rpc.register_handler(RPC_CHECK_HOLDER,
        std::bind(&rpc_server::check_holder, this, std::placeholders::_1))
    );

    RDMA_ASSERT(
      rpc.register_handler(RPC_RECOVERY_READY,
        std::bind(&rpc_server::recovery_ready, this, std::placeholders::_1))
    );
  }
 
  ~rpc_server() {
    this->stop_daemon();
  }

  bool start_daemon() {
    running = true;
    asm volatile("" ::: "memory");

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    return (pthread_create(&handler_tid, &attr, &rpc_server::daemon, this) == 0);
  }

  void stop_daemon() {
    if (running) {
      running = false;
      asm volatile("" ::: "memory");
      pthread_join(handler_tid, nullptr);
    }
  }

  bool run_daemon() {
    running = true;
    asm volatile("" ::: "memory");
    daemon(this);
    return true;
  }

  static void *daemon(void *ctx) {
    rpc_server &ctrl = *((rpc_server *)ctx);
    uint64_t total_reqs = 0;
    while (ctrl.running) {
      total_reqs += ctrl.rpc.run_one_event_loop();
      continue;
    }
    RDMA_LOG(INFO) << "stop with :" << total_reqs << " processed.";
    return nullptr;
  }

private:

  /* Register a lock to the switch.
   *
   * If the lock identifier has been associated with a lock ID,
   * returns the ID without further actions; otherwise, assign
   * a lock ID and install the lock to the data plane.
   */
  ByteBuffer lock_register(const ByteBuffer &b) {
    auto arg = ::rdmaio::Marshal::dedump<lock_register_req>(b);
    if (arg) {
      u64 lock = arg.value().lock_ident;
      auto res = lock_tbl.find(lock);

      // The lock is already registered, return the associated
      // lock id.
      if (res != lock_tbl.end()) {
        return ::rdmaio::Marshal::dump<lock_register_reply>(
          {.lid = res->second});

      // The lock is not registered, register it and return the
      // assigned lock id.
      } else {
        auto lid = ++lid_pool;
        lock_tbl.insert(std::make_pair(lock, lid));

        return ::rdmaio::Marshal::dump<lock_register_reply>(
          {.lid = lid});
      }
    }

    // Should not ever happen.
    return ::rdmaio::Marshal::dump<lock_register_reply>(
      {.lid = 0});
  }

  /* Server requests to start recovery, check whether
   * the switch data plane is ready.
   */
  ByteBuffer recovery_start(const ByteBuffer &b) {
    auto arg = ::rdmaio::Marshal::dedump<recovery_start_req>(b);
    return ::rdmaio::Marshal::dump<recovery_start_reply>(
      {.ready = *ready_signal});
  }

  /* Receive the lock holders sent from servers.
   */
  ByteBuffer collect_holders(const ByteBuffer &b) {
    auto arg = ::rdmaio::Marshal::dedump<collect_holders_req>(b);
    if (arg) lm_state[arg.value().lid].holders.insert(arg.value().holder);

    return ::rdmaio::Marshal::dump<collect_holders_reply>(
      {.error_code = 0});
  }

  /* Receive the lock waiters sent from servers.
   */
  ByteBuffer collect_waiters(const ByteBuffer &b) {
    auto arg = ::rdmaio::Marshal::dedump<collect_waiters_req>(b);
    if (arg) {
      lm_state[arg.value().lid].has_agent = true;
      lm_state[arg.value().lid].waiters.insert(arg.value().waiter);
    }

    return ::rdmaio::Marshal::dump<collect_waiters_reply>(
      {.error_code = 0});
  }

  /* Check whether the waiter exists in the wait queue.
   */
  ByteBuffer check_waiter(const ByteBuffer &b) {
    auto arg = ::rdmaio::Marshal::dedump<check_waiter_req>(b);
    uint8_t ret = 0;

    // If not all living machines have submitted their data,
    // return an error code.
    if (!fault_signal || (ready_bitmap | 1) != (fault_signal | 1)) ret = 2;

    // Try to find the waiter in the wqueue. If found, return exist.
    else if (arg) {
      if (lm_state[arg.value().lid].has_agent) {
        auto res = lm_state[arg.value().lid].waiters.find(arg.value().waiter);
        if (res != lm_state[arg.value().lid].waiters.end()) ret = 1;
      }
    }

    printf("check waiter 0x%lx of lock %d result: %d\n", arg.value().waiter,
      arg.value().lid, ret);

    if (ret == 2) printf("ready_bitmap: %lx, fault_signal: %lx\n", 
      ready_bitmap, fault_signal);

    return ::rdmaio::Marshal::dump<check_waiter_reply>(
      {.exist = ret});
  }

  /* Check whether there are lost holders because of packet loss.
   */
  ByteBuffer check_holder(const ByteBuffer &b) {
    auto arg = ::rdmaio::Marshal::dedump<check_holder_req>(b);
    check_holder_reply reply = {.extra_num = 0};

    if (arg) {
      auto &holders = lm_state[arg.value().lid].holders;

      // The agent wants to compare the number of collected holders 
      // and recorded holders. If there are uncollected holders, 
      // send all holders to the agent as a reply.
      if (arg.value().is_agent) {
        if (holders.size() > arg.value().holder_num) {
          reply.extra_num = holders.size();
          if (reply.extra_num > MAX_HOLDERS) {
            fprintf(stderr, "Error: holder num overflow\n");
            return ::rdmaio::Marshal::dump<check_holder_reply>(reply);
          }

          size_t i = 0;
          for (auto h : holders) reply.extra_holders[i++] = h;
        }

      // A server wants to check whether it needs to restore a lost agent.
      } else {

        // If the agent is really lost, return all holders to the server.
        // Mark the lock as has agent to prevent subsequent requesters from
        // restoring the agent again.
        if (!lm_state[arg.value().lid].has_agent) {
          if (holders.size() > 0) {
            reply.extra_num = holders.size();
            size_t i = 0;
            for (auto h : holders) reply.extra_holders[i++] = h;

            lm_state[arg.value().lid].has_agent = true;
          }
        }
      }
    }

    return ::rdmaio::Marshal::dump<check_holder_reply>(reply);
  }

  /* Mark the requesting machine as ready, which means it has submitted
   * all of its information.
   */
  ByteBuffer recovery_ready(const ByteBuffer &b) {
    auto arg = ::rdmaio::Marshal::dedump<recovery_ready_req>(b);
    if (arg) ready_bitmap |= (1 << arg.value().mid);

    fault_signal = arg.value().fault_code;
    return ::rdmaio::Marshal::dump<recovery_ready_reply>(
      {.ready = (
        *ready_signal != 0 && 
        (ready_bitmap | 1 == fault_signal | 1)
      )});
  }
};

rpc_server* rpc_srv;

int main() {
  rpc_srv = new rpc_server(RPC_SERVER_PORT);

  int fd = open("/tmp/switch_ready", O_RDWR | O_CREAT, 0644);
  ASSERT(ftruncate(fd, 4096) == 0);
  ready_signal = (uint8_t *)mmap(NULL, 4096, PROT_READ | PROT_WRITE,
    MAP_SHARED, fd, 0);
  *ready_signal = 0;
  printf("ready signal: %d\n", *ready_signal);

  rpc_srv->run_daemon();
  return 0;
}