#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>

#include "net.h"
#include "statistics.h"
#include "debug.h"
#include "coroutine_wait_queue.h"
#include "types.h"
#include "lock.h"

/* Header template */
uint8_t header_template[
  sizeof(struct rte_ether_hdr)
  + sizeof(struct rte_ipv4_hdr)
  + sizeof(struct rte_udp_hdr)
  + sizeof (lock_post_header)];
uint16_t ip_hdr_total_length = sizeof(header_template) - sizeof(struct rte_ether_hdr);
uint16_t udp_hdr_dgram_len = sizeof(header_template) - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr);

/* Hardcoded */
struct rte_ether_addr mac_src_addr = {.addr_bytes = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}};
struct rte_ether_addr mac_dst_addr = {.addr_bytes = {0x01, 0x11, 0x22, 0x33, 0x44, 0x01}};

/* Cluster config */
const uint8_t host_list[8] = {1, 2, 3, 4, 5, 6, 7, 8};
const char* ip_list[8] = {
  "10.0.2.4", // pro0_1
  "10.0.2.1", // pro1_1
  "10.0.2.2", // pro2_1
  "10.0.2.3", // pro3_1
  "10.0.2.5", // pro0_2
  "10.0.2.6", // pro1_2
  "10.0.2.7", // pro2_2
  "10.0.2.8" // pro3_2
};

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

int packet_dispatch(lock_post_header* message_header) {
  uint32_t lcore_id = rte_lcore_id();
  if (message_header->op_type == GRANT_LOCK_FROM_SERVER 
    || message_header->op_type == DIRECT_GRANT_FROM_SWITCH) {
      if (!dispatchers[POST_LOCK_GRANT_WO_AGENT]) return 0;
      else (*dispatchers[POST_LOCK_GRANT_WO_AGENT])(message_header, lcore_id);
  } else if (message_header->op_type == MEM_DIFF) {
    if (!dispatchers[POST_MEM_DIFF]) return 0;
    else (*dispatchers[POST_MEM_DIFF])(message_header, lcore_id);
  }
  return 0;
}

/* Packet modifier hooks.
 */
static modifier_f packet_modify;

void register_packet_modifier(modifier_f f) {
  packet_modify = f;
}

void init_header_template() {
  memset(header_template, 0, sizeof(header_template));
  struct rte_ether_hdr *eth_hdr = (struct rte_ether_hdr *)header_template;
  struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((uint8_t*) eth_hdr + sizeof(struct rte_ether_hdr));
  struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
  lock_post_header *message_hdr = (lock_post_header*)((uint8_t*)udp_hdr + sizeof(struct rte_udp_hdr));

  // eth header
  eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
  rte_ether_addr_copy(&mac_src_addr, &eth_hdr->src_addr);
  rte_ether_addr_copy(&mac_dst_addr, &eth_hdr->dst_addr);

  // ip header
  inet_pton(AF_INET, ip_list[LOCALHOST_ID - 1], &(ip_hdr->src_addr));
  inet_pton(AF_INET, ip_list[0], &(ip_hdr->dst_addr));
  ip_hdr->total_length = rte_cpu_to_be_16(ip_hdr_total_length);
  ip_hdr->version = 0x4;
  ip_hdr->ihl = 0x5;
  ip_hdr->next_proto_id = IPPROTO_UDP;

  // udp header
  udp_hdr->src_port = htons(LK_PORT); // TODO: maybe can modify this? use dst port to determine lock header
  udp_hdr->dst_port = htons(LK_PORT);
  udp_hdr->dgram_len = rte_cpu_to_be_16(udp_hdr_dgram_len);

  message_hdr->recirc_flag = 0;
  message_hdr->op_type = 0; // TODO
  message_hdr->mode = 0; // TODO
  message_hdr->client_id = LOCALHOST_ID;
  message_hdr->txn_id = 0; // TODO
  message_hdr->lock_id = 0; // TODO
  message_hdr->timestamp = 0;
  message_hdr->empty_slots = 0;
  message_hdr->head = 0;
  message_hdr->tail = 0;
}

int net_poll_packets() {
  struct rte_mbuf* mbuf_received = NULL;
  struct rte_mbuf* mbuf_received_burst[DPDK_RX_BURST_SIZE];
  int nb_rx = dpdk_poll_recvs(mbuf_received_burst);
  for (int i = 0; i < nb_rx; i++) {
    mbuf_received = mbuf_received_burst[i];
    rte_prefetch0(rte_pktmbuf_mtod(mbuf_received, void *));
    struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(rte_pktmbuf_mtod(mbuf_received, uint8_t*) + 
                                    sizeof(struct rte_ether_hdr) +
                                    sizeof(struct rte_ipv4_hdr));
    count_rx();
    lock_post_header* message_header = (lock_post_header*) ((uint8_t *) udp_hdr + sizeof(struct rte_udp_hdr));
    packet_dispatch(message_header);
    rte_pktmbuf_free(mbuf_received);
  }
  return nb_rx;
}

void generate_header(uint32_t lcore_id, struct rte_mbuf *mbuf, 
                    uint32_t txn_id, uint8_t action_type,
                    uint32_t lock_id, uint8_t lock_type) {
  struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
  struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((uint8_t*) eth_hdr + sizeof(struct rte_ether_hdr));
  struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
  lock_post_header *message_hdr = (lock_post_header*)((uint8_t*)udp_hdr + sizeof(struct rte_udp_hdr));
  rte_memcpy(eth_hdr, header_template, sizeof(header_template));
  mbuf->data_len = sizeof(header_template);
  mbuf->pkt_len = sizeof(header_template);

  udp_hdr->src_port = htons(LK_PORT + (get_tx_count_lcore(lcore_id)) % 128);

  message_hdr->lock_id = htonl(lock_id);
  message_hdr->op_type = action_type;
  message_hdr->mode = lock_type;
  message_hdr->txn_id = htonl(txn_id);

  // Apply the packet modifier.
  if (packet_modify != NULL) {
    post_t post_type;
    if (action_type == ACQUIRE_LOCK) post_type = POST_LOCK_ACQUIRE;
    else if (action_type == RELEASE_LOCK) post_type = POST_LOCK_FREE;
    else return; // Invalid action type
    size_t append_size = packet_modify(post_type, message_hdr);
    ip_hdr->total_length = rte_cpu_to_be_16(ip_hdr_total_length + append_size);
    udp_hdr->dgram_len = rte_cpu_to_be_16(udp_hdr_dgram_len + append_size);
    mbuf->data_len += append_size;
    mbuf->pkt_len += append_size;
  }
}

void generate_header_lock_server(
  uint32_t lcore_id, struct rte_mbuf *mbuf, 
  lock_queue_node* node, uint8_t action_type,
  uint8_t transferred, uint8_t ncnt,
  uint16_t udp_dst_port) {
  // DEBUG("server lcore %u called generate_header_lock_server", lcore_id);
  struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(mbuf, struct rte_ether_hdr *);
  struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((uint8_t*) eth_hdr + sizeof(struct rte_ether_hdr));
  struct rte_udp_hdr *udp_hdr = (struct rte_udp_hdr *)((uint8_t*)ip_hdr + sizeof(struct rte_ipv4_hdr));
  lock_post_header *message_hdr = (lock_post_header*)((uint8_t*)udp_hdr + sizeof(struct rte_udp_hdr));
  rte_memcpy(eth_hdr, header_template, sizeof(header_template));
  mbuf->data_len = sizeof(header_template) + node->payload_size;
  mbuf->pkt_len = sizeof(header_template) + node->payload_size;

  ip_hdr->src_addr = node->ip_src;
  ip_hdr->dst_addr = node->ip_src; // ip_src has not been changed endianness, so not change here
  ip_hdr->total_length = rte_cpu_to_be_16(ip_hdr_total_length + node->payload_size);

  udp_hdr->src_port = htons(LK_PORT + (get_tx_count_lcore(lcore_id)) % 128);
  udp_hdr->dst_port = htons(udp_dst_port);
  udp_hdr->dgram_len = rte_cpu_to_be_16(udp_hdr_dgram_len + node->payload_size);

  message_hdr->client_id = node->client_id;
  message_hdr->lock_id = htonl(node->lock_id);
  message_hdr->op_type = action_type;
  message_hdr->mode = node->mode;
  message_hdr->txn_id = htonl(node->txn_id);
  message_hdr->transferred = transferred;
  message_hdr->ncnt = ncnt;

  // Write payload into mbuf if payload exists
  if (node->payload_size > 0) {
    void* payload_ptr = (void*)((uint8_t*)message_hdr + sizeof(lock_post_header));
    memcpy(payload_ptr, node->payload, node->payload_size);
  }
}

void net_send(
  uint32_t lcore_id, uint32_t txn_id, 
  uint8_t action_type, uint32_t lock_id, uint8_t lock_type) {
  struct rte_mbuf* mbuf = (struct rte_mbuf*)dpdk_get_mbuf();
  if (mbuf == NULL) {
      ERROR("dpdk alloc mbuf fail");
  }

  generate_header(lcore_id, mbuf, txn_id, action_type, lock_id, lock_type);
  int tx_count = dpdk_send(mbuf);
  if (unlikely(tx_count != 1)) {
    ERROR("net_send failed");
    rte_pktmbuf_free(mbuf);
  } else {
    count_tx();
  }
}

void net_send_lock_server(
  uint32_t lcore_id, lock_queue_node* node, 
  uint8_t action_type, uint8_t transferred,
  uint8_t ncnt) {
  struct rte_mbuf* mbuf = (struct rte_mbuf*)dpdk_get_mbuf();
  if (mbuf == NULL) {
    ERROR("dpdk alloc mbuf fail");
  }

  generate_header_lock_server(lcore_id, mbuf, node, action_type, transferred, ncnt, LK_PORT);
  int tx_count = dpdk_send(mbuf);
  if (unlikely(tx_count != 1)) {
    rte_pktmbuf_free(mbuf);
  } else {
    count_tx();
  }
}

void net_broadcast_lock_server(
  uint32_t lcore_id, lock_queue_node* node, 
  uint8_t action_type) {
  int n = HOST_NUM - 1;
  struct rte_mbuf* bufs[DPDK_TX_BURST_SIZE];

  // Broadcast to all hosts in the cluster
  int cnt = 0;
  for (int i = 0; i < HOST_NUM; i++) {
    if (host_list[i] == LOCALHOST_ID) continue;
    struct rte_mbuf* cur_buf = (struct rte_mbuf*)dpdk_get_mbuf();
    if (cur_buf == NULL) {
      ERROR("dpdk alloc mbuf fail");
    }
    if (action_type == MEM_DIFF) {
      generate_header_lock_server(lcore_id, cur_buf, node, action_type, 0, 0, MEM_DIFF_PORT);
    } else {
      generate_header_lock_server(lcore_id, cur_buf, node, action_type, 0, 0, LK_PORT);
    }
    // Change dest ip addr to current target host
    struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(cur_buf, struct rte_ether_hdr *);
    struct rte_ipv4_hdr *ip_hdr = (struct rte_ipv4_hdr *)((uint8_t*) eth_hdr + sizeof(struct rte_ether_hdr));
    inet_pton(AF_INET, ip_list[i], &(ip_hdr->dst_addr));
    bufs[cnt++] = cur_buf;
  }

  int tx_count = dpdk_send_batch(bufs, n);
  ASSERT_MSG(tx_count == n, "net_broadcast_lock_server failed, only sent %d packets", tx_count);
  multi_count_tx(tx_count);
  for (int i = tx_count; i < n; i++) rte_pktmbuf_free(bufs[i]);
  return;
}

int rpc_setup(const char* server_addr) {
  return 0;
}
