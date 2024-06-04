
parser IngressParser(
	packet_in pkt,
	out header_t hdr,
	out metadata_t ig_md,
	out ingress_intrinsic_metadata_t ig_intr_md) {
    
	state start {
        pkt.extract(ig_intr_md);
        transition select(ig_intr_md.resubmit_flag) {
            1 : parse_resubmit;
            0 : parse_port_metadata;
        }
	}

    state parse_resubmit {
        // Parse resubmitted packet here.
        transition reject;
    }

    state parse_port_metadata {
        pkt.advance(PORT_METADATA_SIZE);
        transition parse_ethernet;
    }

	state parse_ethernet {
		pkt.extract(hdr.ethernet);
		transition select(hdr.ethernet.l3_proto) {
			TYPE_IPV4: 	parse_ipv4;
			TYPE_IPV6: 	parse_ipv6;
			TYPE_ARP:  	parse_arp;
			default: 	accept;
		}
	}

	state parse_arp {
        pkt.extract(hdr.arp);
        transition accept;
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.l4_proto) {
            TYPE_UDP: 	parse_udp;
            default: 	accept;
        }
    }

    state parse_ipv6 {
        pkt.extract(hdr.ipv6);
        transition accept;
    }

    state parse_udp {
        pkt.extract(hdr.udp);
		transition select(hdr.udp.dst_port) {
            UDP_PORT_ROCE:  	parse_roce;
            UDP_PORT_SERVER: 	parse_lock;
            UDP_PORT_CLIENT: 	parse_lock;
            default: 		    accept;
        }
    }

    state parse_lock {
        pkt.extract(hdr.lock);
        transition accept;
    }

	state parse_roce {
        pkt.extract(hdr.roce);
        ig_md.is_roce = 1;
        ig_md.pkt_psn = hdr.roce.psn;

        transition select(hdr.roce.opcode) {
            ROCE_READ_REQ: parse_roce_reth;
            ROCE_READ_RES: parse_roce_aeth;
            ROCE_WRITE_REQ: parse_roce_reth;
            ROCE_WRITE_RES: parse_roce_aeth;
            default: accept;
        }
    }

	state parse_roce_reth {
        pkt.extract(hdr.roce_reth);
        transition accept;
    }

	state parse_roce_aeth {
        pkt.extract(hdr.roce_aeth);
        transition accept;
    }
}

control IngressDeparser(
	packet_out pkt,
	inout header_t hdr,
	in metadata_t ig_md) {

    Checksum() ipv4_checksum;

    apply {
        hdr.ipv4.hdr_cksum = ipv4_checksum.update({
            hdr.ipv4.version, hdr.ipv4.ihl, hdr.ipv4.diffserv,
            hdr.ipv4.total_len, hdr.ipv4.ident,
            hdr.ipv4.flags, hdr.ipv4.frag_offset, hdr.ipv4.ttl,
            hdr.ipv4.l4_proto, hdr.ipv4.src_ip, hdr.ipv4.dst_ip
        });

        pkt.emit(hdr);
    }
}

parser EgressParser(
	packet_in pkt,
	out header_t hdr,
	out metadata_t eg_md,
    out egress_intrinsic_metadata_t eg_intr_md) {

    state start {
        pkt.extract(eg_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
		pkt.extract(hdr.ethernet);
		transition select(hdr.ethernet.l3_proto) {
			TYPE_IPV4: 	parse_ipv4;
			TYPE_IPV6: 	parse_ipv6;
			TYPE_ARP:  	parse_arp;
			default: 	accept;
		}
	}

	state parse_arp {
        pkt.extract(hdr.arp);
        transition accept;
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.l4_proto) {
            TYPE_UDP: 	parse_udp;
            default: 	accept;
        }
    }

    state parse_ipv6 {
        pkt.extract(hdr.ipv6);
        transition accept;
    }

    state parse_udp {
        pkt.extract(hdr.udp);
		transition select(hdr.udp.dst_port) {
            UDP_PORT_ROCE:  	parse_roce;
            UDP_PORT_SERVER: 	parse_lock;
            UDP_PORT_CLIENT: 	parse_lock;
            default: 		    accept;
        }
    }

    state parse_lock {
        pkt.extract(hdr.lock);
        transition accept;
    }

	state parse_roce {
        pkt.extract(hdr.roce);
        eg_md.is_roce = 1;
        eg_md.pkt_psn = hdr.roce.psn;

        transition select(hdr.roce.opcode) {
            ROCE_READ_REQ: parse_roce_reth;
            ROCE_READ_RES: parse_roce_aeth;
            ROCE_WRITE_REQ: parse_roce_reth;
            ROCE_WRITE_RES: parse_roce_aeth;
            default: accept;
        }
    }

	state parse_roce_reth {
        pkt.extract(hdr.roce_reth);
        transition accept;
    }

	state parse_roce_aeth {
        pkt.extract(hdr.roce_aeth);
        transition accept;
    }
}

control EgressDeparser(
    packet_out pkt,
    inout header_t hdr,
    in metadata_t ig_md,
    in egress_intrinsic_metadata_for_deparser_t ig_intr_dprs_md) {

    apply {
        pkt.emit(hdr);
    }
}


