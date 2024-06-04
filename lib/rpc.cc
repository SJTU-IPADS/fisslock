
#include <mutex>

#include "rpc.h"
#include "debug.h"

#include "core/lib.hh"

using namespace rdmaio::bootstrap;
using namespace std;

SRpc* rpc_client;
mutex rpc_mtx;

/**
 * Issue an RPC call to the RPC daemon.
 *
 * Note that it do this synchronously, i.e., do not return until
 * receiving the rpc reply from the daemon.
 */
void* rpc(rpc_op op, const char* msg, size_t sz, size_t* reply_sz) {
  rpc_mtx.lock();

  auto res = rpc_client->call(op, rdmaio::ByteBuffer(msg, sz));
  ASSERT(res == rdmaio::IOCode::Ok);

  auto res_reply = rpc_client->receive_reply();
  ASSERT_MSG(res_reply == rdmaio::IOCode::Ok, "error: %s", 
    (char *)res_reply.desc.data());

  rpc_mtx.unlock();

  if (reply_sz) *reply_sz = res_reply.desc.size();
  return (void *)res_reply.desc.data();
}

/**
 * Connect to the RPC daemon.
 */
int rpc_setup_cli(const char* server_addr) {

#ifdef FISSLOCK_FAILURE_RECOVERY
  LOG("Connecting to the RPC server at %s", server_addr);
  rpc_client = new SRpc(server_addr);
#endif

  return 0;
}

