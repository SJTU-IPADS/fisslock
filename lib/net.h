
#ifndef __FISSLOCK_NET_H
#define __FISSLOCK_NET_H

/**
 * Frontend of the network module, which provides the abstraction of
 * packet send/recv and environment setup, etc.
 */

#include <types.h>
#include <post.h>
#include <conf.h>

/**
 * UDP destination ports for flow control.
 */
#define SERVER_POST_TYPE     20001
#define CLIENT_POST_TYPE     20002
#define MEM_POST_TYPE        20003

/* Packet dispatcher hooks.
 *
 * Any module that wants to receive packets from the net module
 * must register a dispatcher function with corresponding post
 * type beforehand.
 */
typedef int (*dispatcher_f)(void* buf, uint32_t core);
void register_packet_dispatcher(post_t t, dispatcher_f f);

/* Network backends.
 */
#include "dpdk.h"

#define net_lcore_id() rte_lcore_id()
#define net_memcpy(dst, src, n) rte_memcpy(dst, src, n)
#define net_new_buf() dpdk_get_mbuf()
#define net_new_buf_bulk(bufs, n) dpdk_get_mbuf_bulk(bufs, n)
#define net_get_sendbuf(buf_hdl) dpdk_mbuf_to_sendbuf(buf_hdl)

/* Expose C++ interfaces in C manner.
 */

#ifdef __cplusplus
extern "C" {
#endif

int packet_dispatch(post_t t, void* buf, uint32_t core);

int net_send(host_id dest, uint64_t buf_hdl, size_t size);
int net_send_batch(host_id dest, uint64_t* buf_hdls, size_t size, int n);
int net_poll_packets();

void init_header_template();

#ifdef __cplusplus
}
#endif

#endif