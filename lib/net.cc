
#include <vector>

#include <stdlib.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
#include <rte_eal.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "net.h"
#include "debug.h"
#include "conf.h"
#include "statistics.h"

uint8_t header_template[
  sizeof(struct rte_ether_hdr) + 
  sizeof(struct rte_ipv4_hdr) + 
  sizeof(struct rte_udp_hdr)];

/* Packet dispatcher hooks.
 *
 * Any module that wants to receive packets from the net module
 * must register a dispatcher function with corresponding post
 * type beforehand.
 */
static dispatcher_f dispatchers[NUM_POST_TYPES];

void register_packet_dispatcher(post_t t, dispatcher_f f) {
  dispatchers[t] = f;
}

int packet_dispatch(post_t t, void* buf, uint32_t core) {
  if (!dispatchers[t]) return 0;
  return (*dispatchers[t])(buf, core);
}

int net_send(host_id dest, uint64_t buf_hdl, size_t size) {
  uint32_t lcore_id = rte_lcore_id();
  struct rte_mbuf* mbuf = (struct rte_mbuf*)buf_hdl;

  struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr*);
  rte_memcpy(eth_hdr, header_template, sizeof(header_template));

  struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((uint8_t*) eth_hdr + sizeof(struct rte_ether_hdr));
  ip_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + size + sizeof(struct rte_udp_hdr));

  struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
  udp_hdr->src_port = rte_cpu_to_be_16(SERVER_POST_TYPE + (get_tx_count_lcore(lcore_id) % 128));
  udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + size);

  mbuf->data_len = size + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);
  mbuf->pkt_len = size + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);

  int tx_cnt = dpdk_send(mbuf);
  if (unlikely(tx_cnt != 1)) {
    rte_pktmbuf_free(mbuf);
  } else {
    count_tx();
  }
  return 0;
}

int net_send_batch(host_id dest, uint64_t* buf_hdls, size_t size, int n) {
  uint32_t lcore_id = rte_lcore_id();
  struct rte_mbuf* bufs[DPDK_TX_BURST_SIZE];

  for (int i = 0; i < n; i++) {
    struct rte_mbuf* mbuf = (struct rte_mbuf*)(buf_hdls[i]);
    struct rte_ether_hdr* eth_hdr = 
      rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr*);
    rte_memcpy(eth_hdr, header_template, sizeof(header_template));

    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)
      ((uint8_t*)eth_hdr + sizeof(struct rte_ether_hdr));
    ip_hdr->total_length = rte_cpu_to_be_16(
      sizeof(struct rte_ipv4_hdr) + size + sizeof(struct rte_udp_hdr));

    struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)
      ((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
    udp_hdr->src_port = rte_cpu_to_be_16(
      SERVER_POST_TYPE + (get_tx_count_lcore(lcore_id) % 128));
    udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + size);

    mbuf->data_len = size + sizeof(struct rte_ether_hdr) + 
      sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);
    mbuf->pkt_len = size + sizeof(struct rte_ether_hdr) + 
      sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr);

    bufs[i] = mbuf;
  }

  int tx_cnt = dpdk_send_batch(bufs, n);
  multi_count_tx(tx_cnt);
  for (int i = tx_cnt; i < n; i++) rte_pktmbuf_free(bufs[i]);
  return 0;
}

int net_poll_packets() {
  uint32_t lcore_id = rte_lcore_id();
  struct lcore_configuration *lconf = &lcore_conf[lcore_id];
  struct rte_mbuf *bufs[DPDK_RX_BURST_SIZE];

  // Receive a burst of packets.
  int rx_cnt = dpdk_poll_recvs(bufs);
  set_burst_time();

  for (int i = 0; i < rx_cnt; i++) {
    rte_prefetch0(rte_pktmbuf_mtod(bufs[i], void *));
    void* receive_buf = rte_pktmbuf_mtod(bufs[i], void*);
    void* packet_buf  = (void*)((uint8_t*)receive_buf + 
                        sizeof(struct rte_ether_hdr) + 
                        sizeof(struct rte_ipv4_hdr) + 
                        sizeof(struct rte_udp_hdr));

    // Dispatch the packet to the corresponding packet buffer.
    int ret = packet_dispatch(
      (post_t)(((post_header *)packet_buf)->type), packet_buf, lcore_id);
    
    // If the packet need to be forwarded back, we re-use the mbuf.
    if (ret) {
      struct rte_mbuf* send_bufs[1];
      send_bufs[0] = bufs[i];

      // If the payload is modified, enlarge the pkt to the modified size.
      if (ret > 1) {
        bufs[i]->data_len += ret;
        bufs[i]->pkt_len += ret;
        struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)((uint8_t*)receive_buf + 
                                      sizeof(struct rte_ether_hdr));
        ip_hdr->total_length = rte_be_to_cpu_16(ip_hdr->total_length);
        ip_hdr->total_length += ret;
        ip_hdr->total_length = rte_cpu_to_be_16(ip_hdr->total_length);
      }

      uint16_t tx_cnt = rte_eth_tx_burst(
        port, lconf->tx_queue_id[0], send_bufs, 1);
      assert(tx_cnt == 1);
      continue;
    }
    rte_pktmbuf_free(bufs[i]);
  }
  return rx_cnt;
}

void init_header_template() {
  memset(header_template, 0, sizeof(header_template));
  struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *)header_template;
  eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
  struct rte_ether_addr eth_src_addr = {.addr_bytes = {0x08, 0xc0, 0xeb, 0xdc, 0xb3, 0x00}};
  struct rte_ether_addr eth_dst_addr = {.addr_bytes = {0x08, 0xc0, 0xeb, 0xdc, 0xa1, 0x12}};
  rte_ether_addr_copy(&eth_src_addr, &(eth_hdr->src_addr));
  rte_ether_addr_copy(&eth_dst_addr, &(eth_hdr->dst_addr));

  struct rte_ipv4_hdr* ip_hdr= (struct rte_ipv4_hdr*)((uint8_t*)eth_hdr + sizeof(struct rte_ether_hdr));
  ip_hdr->src_addr = 167772672;
  ip_hdr->dst_addr = 167772673;
  ip_hdr->version = 0x4;
  ip_hdr->ihl = 0x5;  
  ip_hdr->next_proto_id = IPPROTO_UDP;

  struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
  udp_hdr->dst_port = rte_cpu_to_be_16(SERVER_POST_TYPE);
}
