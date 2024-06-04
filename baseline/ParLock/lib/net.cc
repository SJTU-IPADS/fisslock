#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>

#include "net.h"
#include "dpdk.h"
#include "types.h"
#include "debug.h"
#include "statistics.h"

uint8_t header_template [sizeof(struct rte_ether_hdr)
                        + sizeof(struct rte_ipv4_hdr)
                        + sizeof(struct rte_udp_hdr)
                        + sizeof(lock_post_header)];

const uint8_t mac_list[8][6] = {
    {0x10, 0x70, 0xfd, 0x0d, 0xe2, 0x30}, // pro0_1
    {0x08, 0xc0, 0xeb, 0xfd, 0x52, 0xc4}, // pro1_1
    {0x08, 0xc0, 0xeb, 0xfe, 0x8c, 0x50}, // pro2_1
    {0x10, 0x70, 0xfd, 0x0d, 0xe4, 0x48}, // pro3_1
    {0x08, 0xc0, 0xeb, 0xde, 0x01, 0x56}, // pro0_2
    {0x08, 0xc0, 0xeb, 0xdc, 0xb3, 0x0a}, // pro1_2
    {0x08, 0xc0, 0xeb, 0xde, 0x01, 0x1a}, // pro2_2
    {0x08, 0xc0, 0xeb, 0xdc, 0xa1, 0x5a} // pro3_2
};
const uint8_t host_list[HOST_NUM] = {1, 2, 3, 4, 5, 6, 7, 8};

#ifdef PARLOCK_ZIPF_LOAD_BALANCE
const uint32_t lock_map[HOST_NUM * 2] = {
    0, 13,
    14, 182,
    183, 1415,
    1416, 7757,
    7758, 33169,
    33170, 117911,
    117912, 363525,
    363526, 999999
};
#endif

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

int packet_dispatch(post_t t, lock_post_header* buf, uint32_t core) {
  return (*dispatchers[t])(buf, core);
}

static struct rte_ether_addr* host_id_to_mac(uint8_t id) {
    int index = 0;
    for (index = 0; index < 8; index++) {
        if (host_list[index] == id) {
            break;
        }
    }
    struct rte_ether_addr *result = (struct rte_ether_addr*)malloc(sizeof(struct rte_ether_addr));
    for (int i = 0; i < 6; i++) {
        result->addr_bytes[i] = mac_list[index][i];
    }
    return result;
}

void init_header_template() {
  memset(header_template, 0, sizeof(header_template));
  struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *)header_template;
  struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((uint8_t*) eth_hdr + sizeof(struct rte_ether_hdr));
  struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
  lock_post_header *lk_hd = (lock_post_header*)((uint8_t*)udp_hdr + sizeof(struct rte_udp_hdr));

  eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
  struct rte_ether_addr eth_src_addr;
  struct rte_ether_addr eth_dst_addr;
  for (int i = 0; i < 6; i++) {
    eth_src_addr.addr_bytes[i] = mac_list[LOCALHOST_ID - 1][i];
    eth_dst_addr.addr_bytes[i] = mac_list[LOCALHOST_ID - 1][i];
  }
  rte_ether_addr_copy(&eth_src_addr, &(eth_hdr->src_addr));
  rte_ether_addr_copy(&eth_dst_addr, &(eth_hdr->dst_addr));

	ip_hdr->src_addr = 167772672;
  ip_hdr->dst_addr = 167772673;
  ip_hdr->total_length = rte_cpu_to_be_16(sizeof(struct rte_ipv4_hdr) + sizeof(lock_post_header) + sizeof(struct rte_udp_hdr));
  ip_hdr->next_proto_id = IPPROTO_UDP;
  ip_hdr->version = 0x4;
  ip_hdr->ihl = 0x5;

  udp_hdr->src_port = rte_cpu_to_be_16(1234);
  udp_hdr->dst_port = rte_cpu_to_be_16(1234);
  udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(struct rte_udp_hdr) + sizeof(lock_post_header));

  lk_hd->machine_id = 0;
  lk_hd->lh_id = 0;
  lk_hd->lock_id = 0;
  lk_hd->op = 0;
  lk_hd->post_type = 0;
}

int net_poll_packets() {
  uint32_t lcore_id = rte_lcore_id();
  struct rte_mbuf *bufs[DPDK_RX_BURST_SIZE];
  set_burst_time();
  int rx_cnt = dpdk_poll_recvs(bufs);
  for (int i = 0; i < rx_cnt; i++) {
    rte_prefetch0(rte_pktmbuf_mtod(bufs[i], void *));
    struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(rte_pktmbuf_mtod(bufs[i], uint8_t*) + 
                                    sizeof(struct rte_ether_hdr) +
                                    sizeof(struct rte_ipv4_hdr));
    if (unlikely(rte_be_to_cpu_16(udp_hdr->src_port) != 1234)) {
        rte_pktmbuf_free(bufs[i]);
        continue;
    }
    count_rx();
    lock_post_header *buf = (lock_post_header*)((uint8_t*)udp_hdr + sizeof(struct rte_udp_hdr));
    lock_post_header *lk_hd = (lock_post_header*)malloc(sizeof(lock_post_header));
    lk_hd->machine_id = buf->machine_id;
    lk_hd->lh_id = buf->lh_id;
    lk_hd->lock_id = buf->lock_id;
    lk_hd->op = buf->op;
    lk_hd->post_type = buf->post_type;
    rte_pktmbuf_free(bufs[i]);
    packet_dispatch((post_t)(lk_hd->post_type), lk_hd, lcore_id);
    free(lk_hd);
  }
  return rx_cnt;
}

int net_send(uint8_t dest, uint8_t post_type, uint8_t op, uint32_t lock_id, uint8_t machine_id, uint32_t lh_id) {
  uint32_t lcore_id = rte_lcore_id();
  uint32_t current_tx_count = get_tx_count_lcore(lcore_id);

  struct rte_mbuf* mbuf = (struct rte_mbuf*)dpdk_get_mbuf();
  if (mbuf == NULL) {
    ERROR("dpdk alloc mbuf fail");
  }

  struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr*);
  rte_memcpy(eth_hdr, header_template, sizeof(header_template));
  struct rte_ether_addr eth_dst_addr;
  for (int i = 0; i < 6; i++) {
    eth_dst_addr.addr_bytes[i] = mac_list[dest - 1][i];
  }
  rte_ether_addr_copy(&eth_dst_addr, &(eth_hdr->dst_addr));

  struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((uint8_t*) eth_hdr + sizeof(struct rte_ether_hdr));
  uint32_t ip_src_addr, ip_dst_addr;
  ip_src_addr = 167772672 + current_tx_count % 256;
  ip_dst_addr = 167772672 + current_tx_count % 256;
  ip_hdr->src_addr = ip_src_addr;
  ip_hdr->dst_addr = ip_dst_addr;

  struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
  if (post_type == POST_LOCK_REQ || post_type == POST_UNLOCK_REQ) {
    udp_hdr->dst_port = rte_cpu_to_be_16(SERVER_POST_TYPE);
  } else {
    udp_hdr->dst_port = rte_cpu_to_be_16(CLIENT_POST_TYPE);
  }
    
  lock_post_header *lk_hd = (lock_post_header*)((uint8_t*)udp_hdr + sizeof(struct rte_udp_hdr));
  lk_hd->machine_id = machine_id;
  lk_hd->lh_id = lh_id;
  lk_hd->lock_id = lock_id;
  lk_hd->op = op;
  lk_hd->post_type = post_type;

  mbuf->data_len = sizeof(header_template);
  mbuf->pkt_len = sizeof(header_template);

  int tx_count = dpdk_send(mbuf);
  if (unlikely(tx_count != 1)) {
    rte_pktmbuf_free(mbuf);
    assert(0);
  } else {
    count_tx();
  }
  return 0;
}

uint8_t get_lock_server(uint32_t lock_id) {
#ifdef PARLOCK_TPCC_SHUFFLE
  return (lock_id % HOST_NUM) + 1;
#else
#ifdef PARLOCK_ZIPF_LOAD_BALANCE
  for (uint8_t host_id = 1; host_id <= HOST_NUM; host_id++) {
    if (lock_id >= lock_map[2 * (host_id - 1)] && lock_id <= lock_map[2 * (host_id - 1) + 1]) {
      return host_id;
    }
  }
#else
  int lock_per_host = MAX_LOCK_NUM / HOST_NUM;
  int server_list_index = lock_id / lock_per_host;
  if (server_list_index >= HOST_NUM) {
      server_list_index = HOST_NUM - 1;
  }
  return host_list[server_list_index];
#endif
#endif
}

int rpc_setup(const char* server_addr) {
  return 0;
}
