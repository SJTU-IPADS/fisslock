#ifndef __FISSLOCK_DPDK_H
#define __FISSLOCK_DPDK_H

#include <rte_mbuf.h>

#include "conf.h"

#define DPDK_RX_RING_SIZE       8192
#define DPDK_TX_RING_SIZE       8192
#define DPDK_RX_QUEUE_PER_LCORE 1
#define DPDK_TX_QUEUE_PER_LCORE 1
#define DPDK_NUM_MBUFS          524287
#define DPDK_MBUF_CACHE_SIZE    250
#define DPDK_RX_BURST_SIZE      32
#define DPDK_TX_BURST_SIZE      1024

struct lcore_configuration {
    uint32_t rx_queue_id[DPDK_RX_QUEUE_PER_LCORE];
    uint32_t tx_queue_id[DPDK_TX_QUEUE_PER_LCORE];
} __rte_cache_aligned;

extern struct lcore_configuration lcore_conf[DPDK_LCORE_NUM];

extern uint16_t port;

#define RSS_HASH_KEY_LENGTH 40
extern uint8_t rss_hash_key_symmetric[RSS_HASH_KEY_LENGTH];
extern uint8_t rss_hash_key_asymmetric[RSS_HASH_KEY_LENGTH];
extern struct rte_eth_conf port_conf_template;

#ifdef __cplusplus
extern "C" {
#endif

int dpdk_setup(int argc, char* argv[]);
uint64_t dpdk_get_mbuf();
void dpdk_get_mbuf_bulk(uint64_t* bufs, uint32_t n);
char* dpdk_mbuf_to_sendbuf(uint64_t buf_hdl);
int dpdk_send(struct rte_mbuf* mbuf);
int dpdk_send_batch(struct rte_mbuf** bufs, int n);
int dpdk_poll_recvs(struct rte_mbuf** bufs);

void register_flow(rte_be16_t udp_dst_port, int lcore_left, int lcore_right);

#ifdef __cplusplus
}
#endif

#endif