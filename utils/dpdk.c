#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <arpa/inet.h>

#include "net.h"
#include "conf.h"
#include "debug.h"
#include "statistics.h"
#include "dpdk.h"
#include "lock.h"

struct rte_mempool *pktmbuf_pool = NULL;
uint16_t port = 0; /* The ethernet port */
struct lcore_configuration lcore_conf[DPDK_LCORE_NUM] = {0};

uint8_t rss_hash_key_symmetric[RSS_HASH_KEY_LENGTH] = {
  0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
  0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
  0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
  0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
  0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
};
uint8_t rss_hash_key_asymmetric[RSS_HASH_KEY_LENGTH] = { 
  0x6d, 0x5a, 0x56, 0xda, 0x25, 0x5b, 0x0e, 0xc2, 
  0x41, 0x67, 0x25, 0x3d, 0x43, 0xa3, 0x8f, 0xb0, 
  0xd0, 0xca, 0x2b, 0xcb, 0xae, 0x7b, 0x30, 0xb4, 
  0x77, 0xcb, 0x2d, 0xa3, 0x80, 0x30, 0xf2, 0x0c, 
  0x6a, 0x42, 0xb7, 0x3b, 0xbe, 0xac, 0x01, 0xfa
}; 
struct rte_eth_conf port_conf_template = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
		.split_hdr_size = 0,
		.offloads = DEV_RX_OFFLOAD_IPV4_CKSUM,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = rss_hash_key_asymmetric,
			.rss_key_len = RTE_DIM(rss_hash_key_asymmetric),
			.rss_hf = (ETH_RSS_IP | ETH_RSS_UDP),
		},
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
		.offloads = DEV_TX_OFFLOAD_IPV4_CKSUM | DEV_TX_OFFLOAD_UDP_CKSUM,
	},
};

uint64_t dpdk_get_mbuf() {
  return (uint64_t)rte_pktmbuf_alloc(pktmbuf_pool);
}

void dpdk_get_mbuf_bulk(uint64_t* bufs, uint32_t n) {
  struct rte_mbuf** mbufs = (struct rte_mbuf**)bufs;
  rte_pktmbuf_alloc_bulk(pktmbuf_pool, mbufs, n);
}

char* dpdk_mbuf_to_sendbuf(uint64_t buf_hdl) {
  struct rte_mbuf* mbuf = (struct rte_mbuf*)buf_hdl;
  struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr*);
  char* send_buf = (char*)eth_hdr + sizeof(struct rte_ether_hdr) + 
                  sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);
  return send_buf;
}

/* The input `buf` will be freed in this function,
 * so the `buf` should not be used anymore after calling this function
 */
int dpdk_send(struct rte_mbuf* mbuf) {
  uint32_t lcore_id = rte_lcore_id();
  struct lcore_configuration *lconf = &lcore_conf[lcore_id];
  struct rte_mbuf* bufs[1];

  bufs[0] = mbuf;
  const uint16_t tx_cnt = rte_eth_tx_burst(port, lconf->tx_queue_id[0], bufs, 1);
  return tx_cnt;
}

int dpdk_send_batch(struct rte_mbuf** bufs, int n) {
  uint32_t lcore_id = rte_lcore_id();
  struct lcore_configuration *lconf = &lcore_conf[lcore_id];

  const uint16_t tx_cnt = rte_eth_tx_burst(port, lconf->tx_queue_id[0], bufs, n);
  return tx_cnt;
}

int dpdk_poll_recvs(struct rte_mbuf** bufs) {
  uint32_t lcore_id = rte_lcore_id();
  struct lcore_configuration *lconf = &lcore_conf[lcore_id];

  // Receive a burst of packets.
  const uint16_t rx_cnt = rte_eth_rx_burst(port, lconf->rx_queue_id[0], bufs, DPDK_RX_BURST_SIZE);
  return rx_cnt;
}

static inline int port_init(uint16_t portid, struct rte_mempool *mbuf_pool, struct rte_eth_conf port_conf) {
  uint16_t nb_rxd = DPDK_RX_RING_SIZE;
  uint16_t nb_txd = DPDK_TX_RING_SIZE;
  struct rte_eth_dev_info dev_info;
  struct rte_eth_txconf txconf;
  struct rte_eth_rxconf rxconf;
  int retval;
  uint16_t q;

  if (!rte_eth_dev_is_valid_port(portid))
    return -1;

  retval = rte_eth_dev_info_get(portid, &dev_info);
  if (retval != 0) return retval;

  if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
    port_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
  port_conf.rx_adv_conf.rss_conf.rss_hf &= dev_info.flow_type_rss_offloads;

  uint32_t rx_queue_id = 0;
  uint32_t tx_queue_id = 0;
  for (int i = 0; i < DPDK_LCORE_NUM; i++) {
    if (rte_lcore_is_enabled(i)) {
        lcore_conf[i].rx_queue_id[0] = rx_queue_id++;
        lcore_conf[i].tx_queue_id[0] = tx_queue_id++;
    }
  }

  /* Configure the Ethernet device. */
  retval = rte_eth_dev_configure(
    portid, DPDK_LCORE_NUM, DPDK_LCORE_NUM, &port_conf);
  if (retval) return retval;

  retval = rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd, &nb_txd);
  if (retval) return retval;

  /* Allocate and set up RX queues. */
  rxconf = dev_info.default_rxconf;
  rxconf.offloads = port_conf.rxmode.offloads;
  for (q = 0; q < DPDK_LCORE_NUM; q++) {
    retval = rte_eth_rx_queue_setup(portid, q, nb_rxd, 
      rte_eth_dev_socket_id(portid), &rxconf, mbuf_pool);
    if (retval < 0) return retval;
  }

  /* Allocate and set up TX queues. */
  txconf = dev_info.default_txconf;
  txconf.offloads = port_conf.txmode.offloads;
  for (q = 0; q < DPDK_LCORE_NUM; q++) {
    retval = rte_eth_tx_queue_setup(portid, q, nb_txd, 
      rte_eth_dev_socket_id(portid), &txconf);
    if (retval < 0) return retval;
  }

  /* Start Ethernet port. */
  retval = rte_eth_dev_start(portid);
  if (retval < 0) return retval;

  /* Enable RX in promiscuous mode for the Ethernet device. */
  retval = rte_eth_promiscuous_enable(portid);
  if (retval != 0) return retval;
  return 0;
}

/** 
 * Register a flow in the rte flow table.
 * 
 * After the registration, all packets that has identical UDP destination
 * port to `udp_dst_port` are distributed among cores in 
 * [lcore_left, lcore_right], inclusive.
 */
void register_flow(rte_be16_t udp_dst_port, int lcore_left, int lcore_right) {
  struct rte_flow_error error;
  struct rte_flow_attr attr = {0};
  attr.ingress = 1;

  struct rte_flow_item rx_pattern[4] = {0};
  struct rte_flow_action rx_action[2] = {0};
  uint16_t rx_core[DPDK_LCORE_NUM] = {0};

  struct rte_flow_item_udp udp_mask = {0};
  struct rte_flow_item_udp udp_spec = {0};
  udp_mask.hdr.dst_port = 0xffff;
  udp_spec.hdr.dst_port = rte_cpu_to_be_16(udp_dst_port);

  rx_pattern[0].type = RTE_FLOW_ITEM_TYPE_ETH;
  rx_pattern[1].type = RTE_FLOW_ITEM_TYPE_IPV4;
  rx_pattern[2].type = RTE_FLOW_ITEM_TYPE_UDP;
  rx_pattern[2].mask = &udp_mask;
  rx_pattern[2].spec = &udp_spec;
  rx_pattern[3].type = RTE_FLOW_ITEM_TYPE_END;

  for (int i = lcore_left; i <= lcore_right; i++) 
    rx_core[i - lcore_left] = i;

  struct rte_flow_action_rss action_rss = {
    .types = (ETH_RSS_IP | ETH_RSS_UDP),
    .key_len = RTE_DIM(rss_hash_key_asymmetric),
    .queue_num = lcore_right - lcore_left + 1,
    .key = rss_hash_key_asymmetric,
    .queue = rx_core,
  };

  rx_action[0].type = RTE_FLOW_ACTION_TYPE_RSS;
  rx_action[0].conf = &action_rss;
  rx_action[1].type = RTE_FLOW_ACTION_TYPE_END;

  int res = rte_flow_validate(port, &attr, rx_pattern, rx_action, &error);
  if (!res) rte_flow_create(port, &attr, rx_pattern, rx_action, &error);
  else ERROR("Invalid flow: %s", error.message);
}

/* Set up the whole dpdk backend of network module.
 *
 * Major tasks include: create mbuf pool; initialize NIC port
 */
int dpdk_setup(int argc, char* argv[]) {
  DEBUG("Setting up DPDK...");
  int ret = rte_eal_init(argc, argv);
  if (ret < 0) ERROR("DPDK EAL initialization failed");

  struct rte_eth_conf port_conf = port_conf_template;

  /* Allocate mempool to hold the mbufs. */
  pktmbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", 
    DPDK_NUM_MBUFS, DPDK_MBUF_CACHE_SIZE, 0, 
    RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (!pktmbuf_pool) ERROR("cannot create mbuf pool");

  /* Initialize the port. */
  uint16_t portid = 0;
  RTE_ETH_FOREACH_DEV(portid) {
    if (port_init(portid, pktmbuf_pool, port_conf) != 0) {
      ERROR("cannot init port %"PRIu16, portid);
    }
    port = portid;
    break;
  }

  init_header_template();
  return 0;
}