#ifndef __PARLOCK_NET_H
#define __PARLOCK_NET_H

#include <stdint.h>
#include "dpdk.h"

#define SERVER_POST_TYPE     30001
#define CLIENT_POST_TYPE     30002

typedef enum {
    POST_LOCK_REQ = 0x1,
    POST_UNLOCK_REQ,
    POST_LOCK_REPLY,
    NUM_POST_TYPES
} post_t;

typedef struct __attribute__((packed)) {
    uint8_t post_type;
    uint8_t op;
    uint32_t lock_id;
    uint8_t machine_id;
    uint32_t lh_id;
} lock_post_header;

typedef int (*dispatcher_f)(lock_post_header* buf, uint32_t core);

#ifdef PARLOCK_ZIPF_LOAD_BALANCE
extern const uint32_t lock_map[];
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define net_lcore_id() rte_lcore_id()

void init_header_template();

int net_poll_packets();
int net_send(uint8_t dest, uint8_t post_type, uint8_t op, uint32_t lock_id, uint8_t machine_id, uint32_t lh_id);

uint8_t get_lock_server(uint32_t lock_id);

void register_packet_dispatcher(post_t t, dispatcher_f f);
int packet_dispatch(post_t t, lock_post_header* buf, uint32_t core);

int rpc_setup(const char* server_addr);

#ifdef __cplusplus
}
#endif

#endif