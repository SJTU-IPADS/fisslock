control SwitchEgress (
	inout header_t hdr,
    inout metadata_t eg_md,
    in egress_intrinsic_metadata_t eg_intr_md,
    in egress_intrinsic_metadata_from_parser_t eg_intr_md_from_prsr,
    inout egress_intrinsic_metadata_for_deparser_t eg_intr_dprs_md,
    inout egress_intrinsic_metadata_for_output_port_t eg_intr_oport_md) {
    // Actions
    action change_mode_act(bit<16> udp_src_port) {
        // hdr.nlk_hdr.mode = eg_md.mode;
        // hdr.nlk_hdr.tid = eg_md.tid;
        // hdr.nlk_hdr.timestamp_lo = eg_md.timestamp_lo;
        // hdr.nlk_hdr.timestamp_hi = eg_md.timestamp_hi;
        // hdr.nlk_hdr.client_id = eg_md.client_id;
        hdr.udp.srcPort = udp_src_port;
    }

    action change_op_type_action() {
        hdr.nlk_hdr.recirc_flag = 0;
        hdr.nlk_hdr.op = ACQUIRE_LOCK;
        hdr.nlk_hdr.head = eg_md.timestamp_lo;
        hdr.nlk_hdr.tail = eg_md.timestamp_hi;
        hdr.nlk_hdr.tid = eg_md.tid;
        hdr.nlk_hdr.mode = eg_md.mode;
        hdr.nlk_hdr.client_id = eg_md.client_id;
        hdr.ipv4.dstAddr = eg_md.ip_address;
        hdr.ipv4.srcAddr = eg_md.ip_address;
        hdr.udp.dstPort = LK_PORT;
    }

    action nop() {

    }

    // Tables
    table change_mode_table {
        key = {
            eg_md.tid: ternary;
        }
        actions = {
            change_mode_act;
        }
        // const default_action = change_mode_act;
        size = 128;
    }

    table test_table {
        key = {
            hdr.ipv4.srcAddr: exact;
        }
        actions = {
            nop;
        }
        size = 16;
        const default_action = nop;
    }

    table change_op_type_table {
        actions = {
            change_op_type_action;
        }
        const default_action = change_op_type_action;
        size = 1;
    }

    apply{
        // if (eg_md.clone_md == 1) {
        //     change_mode_table.apply();
        // }
        test_table.apply();
        change_op_type_table.apply();
        change_mode_table.apply();
    }
}