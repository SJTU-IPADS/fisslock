typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;
typedef bit<16> ether_type_t;

const ether_type_t ETHERTYPE_IPV4 = 16w0x0800;

typedef bit<8> ip_protocol_t;
const ip_protocol_t IP_PROTOCOLS_TCP = 6;
const ip_protocol_t IP_PROTOCOLS_UDP = 17;

typedef bit<8> pkt_type_t;
const pkt_type_t PKT_TYPE_NORMAL = 0;
const pkt_type_t PKT_TYPE_MIRROR = 1;

const bit<3> MIRROR_TYPE_I2E = 1;
const bit<3> MIRROR_TYPE_NORMAL = 0;

const MirrorId_t INVALID_MIRROR_ID = 100;

struct pair {
    bit<32> lo;
    bit<32> hi;
}

header ethernet_t {
    mac_addr_t dstAddr;
    mac_addr_t srcAddr;
    ether_type_t etherType;
}

header ipv4_t {
    bit<4> version;
    bit<4> ihl;
    bit<8> diffserv;
    bit<16> totalLen;
    bit<16> identification;
    bit<3> flags;
    bit<13> fragOffset;
    bit<8> ttl;
    ip_protocol_t protocol;
    bit<16> hdrChecksum;
    ipv4_addr_t srcAddr;
    ipv4_addr_t dstAddr;
}

header tcp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<32> seqNo;
    bit<32> ackNo;
    bit<4> dataOffset;
    bit<3> res;
    bit<3> ecn;
    bit<6> ctrl;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgentPtr;
}

header udp_t {
    bit<16> srcPort;
    bit<16> dstPort;
    bit<16> pkt_length;
    bit<16> checksum;
}

header nlk_hdr_t {
    bit<8> recirc_flag; // 0 means the first time arrive of the release request; 1 means
    bit<8> op; // LOCK or RELEASE
    bit<8> mode; // SHARED or EXCLUSIVE
    bit<8> client_id;
    bit<32> tid;
    bit<32> lock;
    bit<32> timestamp_lo;
    bit<32> timestamp_hi;
    bit<32> empty_slots;
    bit<32> head;
    bit<32> tail;
    bit<8> ncnt;
    bit<8> transferred;
}

header adm_hdr_t {
    bit<8> op;
    bit<32> lock;
    bit<32> new_left;
    bit<32> new_right;
}

header recirculate_hdr_t {
    bit<8> dequeued_mode;
    bit<32> cur_head;
    bit<32> cur_tail;
}

header probe_hdr_t {
    bit<8> failure_status;
    bit<8> op;
    bit<8> mode;
    bit<8> client_id;
    bit<32> tid;
    bit<32> lock;
    bit<32> timestamp_lo;
    bit<32> timestamp_hi;
}



header mirror_hdr_t {
    pkt_type_t pkt_type;
    bit<8> mode;
    bit<8> client_id;
    bit<32> tid;
    bit<32> ip_address;
    bit<32> timestamp_lo;
    bit<32> timestamp_hi;
}

header mirror_bridged_metadata_h {
    pkt_type_t pkt_type;
}


struct header_t {
    mirror_bridged_metadata_h bridged_md;
    ethernet_t ethernet;
    ipv4_t ipv4;
    tcp_t tcp;
    udp_t udp;
    nlk_hdr_t nlk_hdr;
    adm_hdr_t adm_hdr;
    recirculate_hdr_t recirculate_hdr;
    probe_hdr_t probe_hdr;
}

struct metadata_t {
    // bit<32> new_left;
    // bit<32> new_right;
    bit<32> head;
    bit<32> tail;
    bit<32> queue_size_op;
    bit<1> do_resubmit;
    bit<1> routed;
    bit<1> dropped;
    bit<1> lock_exist;
    bit<8> recirc_flag;
    bit<8> dequeued_mode;
    bit<2> recirced;
    bit<32> locked;
    bit<32> left;
    bit<32> right;
    bit<32> src_ip;
    bit<32> dst_ip;
    bit<32> empty_slots;
    bit<32> length_in_server;
    bit<32> size_of_queue;
    bit<32> empty_slots_before_pop;
    bit<32> ts_hi;
    bit<32> ts_lo;
    bit<32> lock_id;
    // bit<1> under_threshold;
    // bit<16> tenant_id;
    // bit<32> tenant_threshold;
    bit<8> failure_status;
    MirrorId_t ing_mir_ses;   // Ingress mirror session ID
    bit<8> clone_md;
    bit<8> mode;
    bit<8> client_id;
    bit<32> tid;
    bit<32> ip_address;
    bit<32> timestamp_lo;
    bit<32> timestamp_hi;
    pkt_type_t pkt_type;
}
