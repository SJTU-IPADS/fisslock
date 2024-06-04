
#ifndef _SWITCH_RPC_H
#define _SWITCH_RPC_H

#include "types.h"
#include "conf.h"

#define MAX_HOLDERS 256

typedef enum {
  RPC_LOCK_REGISTER = 128,
  RPC_RECOVERY_START,
  RPC_COLLECT_HOLDERS,
  RPC_COLLECT_WAITERS,
  RPC_RECOVERY_READY,
  RPC_CHECK_WAITER,
  RPC_CHECK_HOLDER,
} rpc_op;

struct __attribute__((packed)) lock_register_req {
  uint64_t lock_ident;
};

struct __attribute__((packed)) lock_register_reply {
  uint32_t lid;
};

struct __attribute__((packed)) recovery_start_req {
  uint64_t fault_code;
};

struct __attribute__((packed)) recovery_start_reply {
  uint8_t ready;
};

struct __attribute__((packed)) collect_holders_req {
  uint32_t lid;
  uint64_t holder;
};

struct __attribute__((packed)) collect_holders_reply {
  uint8_t error_code;
};

struct __attribute__((packed)) collect_waiters_req {
  uint32_t lid;
  uint64_t waiter;
};

struct __attribute__((packed)) collect_waiters_reply {
  uint8_t error_code;
};

struct __attribute__((packed)) check_waiter_req {
  uint32_t lid;
  uint64_t waiter;
};

struct __attribute__((packed)) check_waiter_reply {
  uint8_t exist;
};

struct __attribute__((packed)) check_holder_req {
  uint8_t  is_agent;
  uint32_t lid;
  uint32_t holder_num;
};

struct __attribute__((packed)) check_holder_reply {
  int32_t extra_num;
  uint64_t extra_holders[MAX_HOLDERS];
};

struct __attribute__((packed)) recovery_ready_req {
  uint32_t mid;
  uint64_t fault_code;
};

struct __attribute__((packed)) recovery_ready_reply {
  uint8_t ready;
};

#ifdef __cplusplus
extern "C" {
#endif

int rpc_setup_cli(const char* server_addr);
void* rpc(rpc_op op, const char* msg, size_t sz, size_t* reply_sz);

#ifdef __cplusplus
}
#endif

#endif