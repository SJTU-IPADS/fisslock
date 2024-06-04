control SwitchIngress(
	inout header_t hdr,
    inout metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {

    Register<bit<32>, bit<1>>(1, 0) switch_counter_register;

    RegisterAction<bit<32>, bit<1>, bit<32>>(switch_counter_register) add_switch_counter_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            value = value + 1;
            result = value;
        }
    };

    action add_switch_counter_action() {
        ig_md.switch_counter = add_switch_counter_alu.execute(0);
    }

    action set_egress(PortId_t egress_spec) {
        ig_intr_tm_md.ucast_egress_port = egress_spec;
        // mirror
        // hdr.bridged_md.setValid();
        // hdr.bridged_md.pkt_type = PKT_TYPE_NORMAL;
        // ig_intr_dprsr_md.mirror_type = MIRROR_TYPE_NORMAL;
        ig_intr_tm_md.bypass_egress = 1w1;
    }

    action fix_src_port_action(bit<16> fix_port) {
        hdr.ethernet.srcAddr = (bit<48>)hdr.ipv4.srcAddr; // TODO
        hdr.udp.srcPort = fix_port;
    }

    action set_as_primary_action() {
        hdr.ethernet.dstAddr = PRIMARY_BACKUP;
    }

    action forward_to_server_action(ipv4_addr_t server_ip) {
        hdr.ipv4.srcAddr = server_ip;
        hdr.ipv4.dstAddr = server_ip;
    }

    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    // Tables
    table add_switch_counter_table {
        actions = {
            add_switch_counter_action;
        }
        const default_action = add_switch_counter_action;
        size = 1;
    }

    table fix_src_port_table {
        key = {
            ig_md.switch_counter: ternary;
        }
        actions = {
            fix_src_port_action;
        }
        size = 256;
    }

    table set_tag_table {
        actions = {
            set_as_primary_action;
        }
        const default_action = set_as_primary_action;
        size = 1;
    }

    table forward_to_server_table {
        key = {
            hdr.nlk_hdr.lock: ternary;
        }
        actions = {
            forward_to_server_action;
        }
        // const default_action = forward_to_server_action;
        size = 32;
    }

    table ipv4_route_table {
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            set_egress;
            drop;
        }
        size = 8192;
        const default_action = drop;
    }

    apply {
        if (hdr.nlk_hdr.isValid() && (hdr.nlk_hdr.op == ACQUIRE_LOCK || hdr.nlk_hdr.op == RELEASE_LOCK)) {
            add_switch_counter_table.apply();
            fix_src_port_table.apply();
            set_tag_table.apply();
            forward_to_server_table.apply();
        }
        if (hdr.nlk_hdr.isValid()) {
            ipv4_route_table.apply();
        }
    }
}