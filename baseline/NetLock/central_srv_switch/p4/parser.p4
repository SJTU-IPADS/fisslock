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
            MEM_DIFF_PORT: parse_nlk_hdr;
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
    
    apply {
       pkt.emit(hdr);
    }
}

parser SwitchEgressParser(
	packet_in pkt,
	out header_t hdr,
	out metadata_t eg_md,
    out egress_intrinsic_metadata_t eg_intr_md) {

    state start {
        transition reject;
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