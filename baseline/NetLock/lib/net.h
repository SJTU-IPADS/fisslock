#ifndef __NETLOCK_NET_H
#define __NETLOCK_NET_H

#include "dpdk.h"
#include "post.h"
#include "lock.h"

typedef int (*dispatcher_f)(void* buf, uint32_t core);
typedef size_t (*modifier_f)(post_t type, lock_post_header* buf);

#ifdef __cplusplus
extern "C" {
#endif

#define net_lcore_id() rte_lcore_id()

void init_header_template();

/* Packet dispatcher hooks.
 *
 * Any module that wants to receive packets from the net module
 * must register a dispatcher function with corresponding post
 * type beforehand.
 */
void register_packet_dispatcher(post_t t, dispatcher_f f);
int packet_dispatch(lock_post_header* message_header);

/* Packet modifier hooks.
 *
 * Any module that wants to apply modifications on packets to
 * send could register a modifier function. The modifier takes
 * the packet type and the end of the packet buffer as arguments,
 * and should return the size of added packet content.
 */
void register_packet_modifier(modifier_f f);

int net_poll_packets();
void generate_header(uint32_t lcore_id, struct rte_mbuf *mbuf, 
                    uint32_t txn_id, uint8_t action_type,
                    uint32_t lock_id, uint8_t lock_type);
void generate_header_lock_server(uint32_t lcore_id, struct rte_mbuf *mbuf, 
                    lock_queue_node* node, uint8_t action_type,
                    uint8_t transferred, uint8_t ncnt,
                    uint16_t udp_dst_port);
void net_send(uint32_t lcore_id, uint32_t txn_id, 
                    uint8_t action_type, uint32_t lock_id, 
                    uint8_t lock_type);
void net_send_lock_server(uint32_t lcore_id, lock_queue_node* node, 
                    uint8_t action_type, uint8_t transferred, uint8_t ncnt);
void net_broadcast_lock_server(uint32_t lcore_id, lock_queue_node* node, 
                    uint8_t action_type);

int rpc_setup(const char* server_addr);

#ifdef __cplusplus
}
#endif

#endif