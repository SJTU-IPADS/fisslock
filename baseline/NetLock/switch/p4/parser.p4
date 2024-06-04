parser SwitchIngressParser(
	packet_in pkt,
	out header_t hdr,
	out metadata_t ig_md,
	out ingress_intrinsic_metadata_t ig_intr_md) {

    TofinoIngressParser() tofino_parser;

    state start {
        tofino_parser.apply(pkt, ig_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            ETHERTYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_TCP: parse_tcp;
            IP_PROTOCOLS_UDP: parse_udp;
            default: accept;
        }
    }

    state parse_tcp {
        pkt.extract(hdr.tcp);
        transition accept;
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition select(hdr.udp.dstPort) {
            PROBE_PORT: parse_probe_hdr;
            LK_PORT: parse_nlk_hdr;
            REPLY_PORT: parse_nlk_hdr;
            ADM_PORT: parse_adm_hdr;
            UDP_WARMUP_DST_PORT: parse_nlk_hdr;
            default: accept;
        }
    }

    state parse_nlk_hdr {
        pkt.extract(hdr.nlk_hdr);
        transition select(hdr.nlk_hdr.recirc_flag) {
            RECIRCULATED_1: parse_recirculate_hdr;
            RECIRCULATED_2: parse_recirculate_hdr;
            default: accept;
        }
    }

    state parse_recirculate_hdr {
        pkt.extract(hdr.recirculate_hdr);
        transition accept;
    }

    state parse_adm_hdr {
        pkt.extract(hdr.adm_hdr);
        transition accept;
    }

    state parse_probe_hdr {
        pkt.extract(hdr.probe_hdr);
        transition accept;
    }
}

control SwitchIngressDeparser(
    packet_out pkt,
    inout header_t hdr,
    in metadata_t ig_md,
    in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
    
    Mirror(0) mirror;

    apply {
        // if (ig_intr_dprsr_md.resubmit_type == DPRSR_DIGEST_TYPE) {
        //     resubmit.emit();
        // }
        // if (ig_intr_dprsr_md.mirror_type == MIRROR_TYPE_I2E) {
        //     mirror.emit<mirror_hdr_t>(ig_md.ing_mir_ses, {ig_md.pkt_type, ig_md.mode, ig_md.client_id, ig_md.tid, ig_md.ip_address, ig_md.timestamp_lo, ig_md.timestamp_hi});
        // }
        mirror.emit<mirror_hdr_t>(ig_md.ing_mir_ses, {ig_md.pkt_type, ig_md.mode, ig_md.client_id, ig_md.tid, ig_md.ip_address, ig_md.timestamp_lo, ig_md.timestamp_hi});
        pkt.emit(hdr);
    }
}

parser SwitchEgressParser(
	packet_in pkt,
	out header_t hdr,
	out metadata_t eg_md,
    out egress_intrinsic_metadata_t eg_intr_md) {
        
    state start {
        pkt.extract(eg_intr_md);
        transition parse_mirror_metadata;
    }

    state parse_mirror_metadata {
        mirror_hdr_t mirror_md = pkt.lookahead<mirror_hdr_t>();
        transition select(mirror_md.pkt_type) {
            PKT_TYPE_NORMAL: parse_bridged_md;
            PKT_TYPE_MIRROR: parse_mirror;
        }
    }

    state parse_bridged_md {
        pkt.extract(hdr.bridged_md);
        transition parse_ethernet;
    }

    state parse_mirror {
        mirror_hdr_t mirror_hdr;
        pkt.extract(mirror_hdr);
        eg_md.clone_md = 1;
        eg_md.mode = mirror_hdr.mode;
        eg_md.tid = mirror_hdr.tid;
        eg_md.timestamp_lo = mirror_hdr.timestamp_lo;
        eg_md.timestamp_hi = mirror_hdr.timestamp_hi;
        eg_md.client_id = mirror_hdr.client_id;
        eg_md.ip_address = mirror_hdr.ip_address;
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.etherType) {
            ETHERTYPE_IPV4: parse_ipv4;
            default: accept;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_TCP: parse_tcp;
            IP_PROTOCOLS_UDP: parse_udp;
            default: accept;
        }
    }

    state parse_tcp {
        pkt.extract(hdr.tcp);
        transition accept;
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition select(hdr.udp.dstPort) {
            LK_PORT: parse_nlk_hdr;
            REPLY_PORT: parse_nlk_hdr;
            default: accept;
        }
    }

    state parse_nlk_hdr {
        pkt.extract(hdr.nlk_hdr);
        transition accept;
    }
}

control SwitchEgressDeparser(
    packet_out pkt,
    inout header_t hdr,
    in metadata_t eg_md,
    in egress_intrinsic_metadata_for_deparser_t eg_intr_dprs_md) {

    apply {
        pkt.emit(hdr);
    }
}
