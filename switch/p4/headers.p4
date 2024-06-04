
////////////////////////////////////////////////////////////////////
// TCP/IP stack headers.
// 

typedef bit<48>  mac_addr_t;
typedef bit<32>  ipv4_addr_t;
typedef bit<128> ipv6_addr_t;
typedef bit<16>  udp_port_t;

typedef bit<16> eth_type;
const eth_type TYPE_IPV4 = 0x0800;
const eth_type TYPE_IPV6 = 0x86dd;
const eth_type TYPE_ARP  = 0x0806;

typedef bit<8> ip_type;
const ip_type TYPE_ICMP = 1;
const ip_type TYPE_TCP  = 6;
const ip_type TYPE_UDP  = 17;

header ethernet_t {
	mac_addr_t dst_mac;
	mac_addr_t src_mac;
	eth_type   l3_proto;
}

header ipv4_t {
	bit<4>      version;
	bit<4>      ihl;
	bit<8>      diffserv;
	bit<16>     total_len;
	bit<16>     ident;
	bit<3>      flags;
	bit<13>     frag_offset;
	bit<8>      ttl;
	bit<8>      l4_proto;
	bit<16>     hdr_cksum;
	ipv4_addr_t src_ip;
	ipv4_addr_t dst_ip;
}

header ipv6_t {
	bit<4>      version;
	bit<8>      traffic_class;
	bit<20>     flow_table;
	bit<16>     payload_len;
	bit<8>      next_hdr;
	bit<8>      hop_limit;
	ipv6_addr_t src_ip;
	ipv6_addr_t dst_ip;
}

header udp_t {
	udp_port_t src_port;
	udp_port_t dst_port;
	bit<16>    len;
	bit<16>    checksum;
}

header arp_t {
	bit<16>     hw_type;
	bit<16>     proto_type;
	bit<8>      hw_addr_len;
	bit<8>      proto_addr_len;
	bit<16>     opcode;
	mac_addr_t  src_mac;
	ipv4_addr_t src_ip;
	mac_addr_t  dst_mac;
	ipv4_addr_t dst_ip;
}

////////////////////////////////////////////////////////////////////
// RoCE headers.
// 
// The RoCE protocol we use is RoCEv2. See the spec here:
// https://docs.nvidia.com/networking/display/WINOFv55053000/RoCEv2
// 

// RoCE OpCodes.
typedef bit<8>  roce_op;
const roce_op ROCE_WRITE_REQ = 10;
const roce_op ROCE_READ_REQ = 12;
const roce_op ROCE_READ_RES = 16;
const roce_op ROCE_WRITE_RES = 17;
const roce_op ROCE_UC_WRITE_REQ = 42;

// RoCE-related data types.
typedef bit<24> qp_t;       /* RDMA Queue Pair */
typedef bit<24> psn_t;      /* Packet Sequence Number */
typedef bit<24> msn_t;      /* Message Sequence Number */
typedef bit<32> key_t;      /* Key for Authentication */
typedef bit<64> mem_addr_t; /* Memory Address */

// RoCEv2 identifies RDMA packets using a specific UDP port
// specified in the UDP header -- 4791.
const udp_port_t UDP_PORT_ROCE = 4791;

// Base RoCE L4 header (BTH).
header roce_t {
	bit<8>  opcode;         /* RDMA Op */
	bit<4>  _unused;        /* includes SE, MigReq, PadCnt */
	bit<4>  trans_hdr_ver;  /* BTH version */
	bit<16> p_key;          /* Associated logical partition key */
	bit<8>  _reserved;
	qp_t    dest_qp;        /* Destination QP Number */
	bit<1>  ack_req;        /* Is ACK required for this packet? */
	bit<7>  __reserved;
	psn_t   psn;            /* Used to detect missing packets */
}

// Extend RoCE L4 headers.
// 
// Depending on the QP and the operation types, there will
// be different extended header formats.

// RETH: for RC QP Read/Write REQ
header roce_reth_t {
	mem_addr_t  vaddr;  /* Remote address to read/write */
	key_t       rkey;   /* Authorize remote memory access */
	bit<32>     length; /* Read/Write data size */
}

// DETH: for UD QP Send/Recv REQ
header roce_deth_t {
	key_t   qkey;       /* Authorize acccesses to the receive queue */
	bit<8>  _reserved;
	qp_t    src_qp;     /* Source QP Number */
}

// AETH: for Any QP ACK
header roce_aeth_t {
	bit<8> syndrome;    /* ACK or NACK? */
	msn_t  msn;         /* Message Sequence Number */
}

////////////////////////////////////////////////////////////////////
// INL headers.
// 

const udp_port_t UDP_PORT_SERVER = 20001;
const udp_port_t UDP_PORT_CLIENT = 20002;
const udp_port_t UDP_PORT_MEMDIFF = 20003;

#define ACQUIRE           0x01
#define GRANT_W_AGENT     0x02
#define GRANT_WO_AGENT    0x03
#define RELEASE           0x04
#define TRANSFER          0x05
#define FREE              0x06

#define MEM_DIFF          0x07

typedef bit<32> lid_t;
typedef bit<8>  host_t;

#define SLICE_SIZE_POW2 19
#define SLICE_SIZE      (1 << SLICE_SIZE_POW2) 
#define SLICE_NUM       8

header lock_hdr_t {
	bit<8>  type;           /* The packet type */
	bit<1>  multicasted;    /* Whether the packet is multicasted */
	bit<1>  granted;        /* Whether the request is granted, valid for agent */
	bit<1>  transferred;    /* Whether the agent is transferred from other host */
	bit<3>  reserved;       
	bit<1>  old_mode;       /* Only valid in transfer/free */
	bit<1>  mode;           /* The requested lock mode */
	lid_t   id;	            /* Lock ID */
	host_t  machine_id;     /* The requester or the next holder host */
	bit<32> task_id;        /* The requester or the next holder task */
	host_t  agent;          /* The lock agent */
	bit<32> wq_size;				/* The wait queue size */
	bit<8>  ncnt;           /* The notification counter */
}

// Lock state: free or acquired.
#define LOCK_FREE     0
#define LOCK_ACQUIRED 1

// Lock state for acquired locks: shared or exclusive.
#define LOCK_SHARED   0
#define LOCK_EXCL     1

////////////////////////////////////////////////////////////////////
// Global data structures.
// 
typedef bit<9> egress_spec_t;

struct metadata_t {
	// Lock
	bit<16>        dest2;
	bit<1>         lock_free_mode;
	bit<1>         lock_rw_mode;
	host_t         lock_agent;
	lid_t          lock_index;
	bit<1>         agent_changed;
	bit<1>         lock_out_of_range;

	// RoCE
	bit<1>   is_roce;
	psn_t    pkt_psn;
}

struct header_t {
	ethernet_t  ethernet;
	ipv4_t      ipv4;
	ipv6_t      ipv6;
	arp_t       arp;
	udp_t       udp;
	roce_t      roce;
	roce_reth_t roce_reth;
	roce_deth_t roce_deth;
	roce_aeth_t roce_aeth;
	lock_hdr_t  lock;
}