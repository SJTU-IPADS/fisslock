control Decode(
	inout header_t hdr,
    inout metadata_t ig_md) {
    // RegisterActions
    RegisterAction<bit<32>, bit<32>, bit<32>>(queue_size_op_register) get_queue_size_op_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<32>>(left_bound_register) get_left_bound_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<32>>(right_bound_register) get_right_bound_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    // Actions
    action decode_action() {
        ig_md.queue_size_op = get_queue_size_op_alu.execute(ig_md.lock_id);
        ig_md.ts_hi = hdr.nlk_hdr.timestamp_hi;
        ig_md.ts_lo = hdr.nlk_hdr.timestamp_lo;
    }

    action get_left_bound_action() {
        ig_md.left = get_left_bound_alu.execute(ig_md.lock_id);
    }

    action get_right_bound_action() {
        ig_md.right = get_right_bound_alu.execute(ig_md.lock_id);
    }

    action get_size_of_queue_action() {
        ig_md.size_of_queue = ig_md.right - ig_md.left;
    }

    // Tables
    @pragma stage 1
    table decode_table {
        actions = {
            decode_action;
        }
        const default_action = decode_action;
        size = 1;
    }

    @pragma stage 1
    table get_left_bound_table {
        actions = {
            get_left_bound_action;
        }
        const default_action = get_left_bound_action;
        size = 1;
    }

    @pragma stage 1
    table get_right_bound_table {
        actions = {
            get_right_bound_action;
        }
        const default_action = get_right_bound_action;
        size = 1;
    }

    @pragma stage 2
    table get_size_of_queue_table {
        actions = {
            get_size_of_queue_action;
        }
        const default_action = get_size_of_queue_action;
        size = 1;
    }

    apply {
        // Stage 0: [register_lo: left_bound_register]
        get_left_bound_table.apply();
        // Stage 0: [register_lo: right_bound_register]
        get_right_bound_table.apply();
        // Stage 0
        get_size_of_queue_table.apply();
        // Stage 1: [register_lo: queue_size_op_register]
        decode_table.apply();
    }
}

control AcquireLock(
	inout header_t hdr,
    inout metadata_t ig_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
    // RegisterActions
    RegisterAction<pair, bit<32>, bit<32>>(slots_two_sides_register) dec_empty_slots_alu = {
        void apply(inout pair value, out bit<32> result) {
            // result = value.lo;
            if (value.lo > 0 && value.hi + ig_md.queue_size_op <= 0) {
                value.lo = value.lo - 1;
                // result = value.hi;
            } else {
                value.hi = value.hi + 1;
                // result = value.hi;
            }
            result = value.hi;
            // result = value.lo;
        }
    };

    RegisterAction<pair, bit<32>, bit<32>>(slots_two_sides_register) push_back_alu = {
        void apply(inout pair value, out bit<32> result) {
            result = value.lo;
            if (value.lo > 0) {
                value.lo = value.lo - 1;
            }
            // value.lo = value.lo - 1;
            value.hi = value.hi - 1;
        }
    };

    RegisterAction<pair, bit<32>, bit<32>>(shared_and_exclusive_count_register) acquire_shared_lock_alu = {
        void apply(inout pair value, out bit<32> result) {
            if (value.lo > 0) {
                value.hi = value.hi + 1;
                result = value.hi;
            } else {
                value.hi = value.hi + 1;
                result = 0;
            }
        }
    };

    RegisterAction<pair, bit<32>, bit<32>>(shared_and_exclusive_count_register) acquire_exclusive_lock_alu = {
        void apply(inout pair value, out bit<32> result) {
            if (value.lo > 0 || value.hi > 0) {
                value.lo = value.lo + 1;
                result = 1;
            } else {
                value.lo = value.lo + 1;
                result = 0;
            }
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<32>>(tail_register) update_tail_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            if (value == ig_md.right) {
                value = ig_md.left;
            } else {
                value = value + 1;
            }
            result = value;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<1>>(ip_array_register) update_ip_array_alu = {
        void apply(inout bit<32> value) {
            value = hdr.ipv4.srcAddr;
        }
    };

    RegisterAction<bit<8>, bit<32>, bit<1>>(mode_array_register) update_mode_array_alu = {
        void apply(inout bit<8> value) {
            value = hdr.nlk_hdr.mode;
        }
    };

    RegisterAction<bit<8>, bit<32>, bit<1>>(client_id_array_register) update_client_id_array_alu = {
        void apply(inout bit<8> value) {
            value = hdr.nlk_hdr.client_id;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<1>>(tid_array_register) update_tid_array_alu = {
        void apply(inout bit<32> value) {
            value = hdr.nlk_hdr.tid;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<1>>(timestamp_hi_array_register) update_timestamp_hi_array_alu = {
        void apply(inout bit<32> value) {
            value = ig_md.ts_hi;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<1>>(timestamp_lo_array_register) update_timestamp_lo_array_alu = {
        void apply(inout bit<32> value) {
            value = ig_md.ts_lo;
        }
    };

    // Actions

    action fix_src_port_action(bit<16> fix_port) {
        hdr.ethernet.srcAddr = (bit<48>)hdr.ipv4.srcAddr; // TODO
        hdr.udp.srcPort = fix_port;
    }

    action set_as_primary_action() {
        hdr.ethernet.dstAddr = PRIMARY_BACKUP;
    }

    action set_as_secondary_action() {
        hdr.ethernet.dstAddr = SECONDARY_BACKUP;
    }

    action set_as_failure_notification_action() {
        hdr.ethernet.dstAddr = FAILURE_NOTIFICATION;
    }

    action forward_to_server_action(ipv4_addr_t server_ip) {
        hdr.ipv4.srcAddr = server_ip;
        hdr.ipv4.dstAddr = server_ip;
        // hdr.ipv4.dstAddr = 0x0a000209;
        hdr.nlk_hdr.empty_slots = ig_md.empty_slots;
        hdr.nlk_hdr.head = ig_md.head;
        hdr.nlk_hdr.tail = ig_md.tail;
    }

    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    action notify_tail_client_action() {
        hdr.ipv4.dstAddr = hdr.ipv4.srcAddr;
        hdr.nlk_hdr.empty_slots = ig_md.length_in_server;
        hdr.nlk_hdr.head = ig_md.head;
        hdr.nlk_hdr.tail = ig_md.tail;
    }

    action switch_direct_grant_action() {
        hdr.nlk_hdr.op = DIRECT_GRANT_FROM_SWITCH;
    }

    action dec_empty_slots_action() {
        ig_md.length_in_server = dec_empty_slots_alu.execute(ig_md.lock_id);
        // ig_md.empty_slots = dec_empty_slots_alu.execute(ig_md.lock_id);
    }

    action push_back_action() {
        ig_md.empty_slots = push_back_alu.execute(ig_md.lock_id);
    }

    action acquire_shared_lock_action() {
        ig_md.locked = acquire_shared_lock_alu.execute(ig_md.lock_id);
    }

    action acquire_exclusive_lock_action() {
        ig_md.locked = acquire_exclusive_lock_alu.execute(ig_md.lock_id);
    }

    action update_tail_action() {
        ig_md.tail = update_tail_alu.execute(ig_md.lock_id);
    }

    action update_ip_array_action() {
        update_ip_array_alu.execute(ig_md.tail);
    }

    action update_mode_array_action() {
        update_mode_array_alu.execute(ig_md.tail);
    }

    action update_client_id_array_action() {
        update_client_id_array_alu.execute(ig_md.tail);
    }

    action update_tid_array_action() {
        update_tid_array_alu.execute(ig_md.tail);
    }

    action update_timestamp_hi_array_action() {
        update_timestamp_hi_array_alu.execute(ig_md.tail);
    }

    action update_timestamp_lo_array_action() {
        update_timestamp_lo_array_alu.execute(ig_md.tail);
    }

    // Tables

    @pragma stage 3
    table dec_empty_slots_table {
        key = {
            hdr.nlk_hdr.op: exact;
        }
        actions = {
            dec_empty_slots_action;
            push_back_action;
        }
        size = 4;
    }

    @pragma stage 4
    table acquire_lock_table {
        key = {
            hdr.nlk_hdr.mode: exact;
        }
        actions = {
            acquire_shared_lock_action;
            acquire_exclusive_lock_action;
        }
        size = 4;
    }

    @pragma stage 4
    table fix_src_port_table {
        key = {
            hdr.nlk_hdr.lock: ternary;
        }
        actions = {
            fix_src_port_action;
        }
        size = 256;
    }

    @pragma stage 4
    table set_tag_table {
        key = {
            ig_md.failure_status: exact;
            ig_md.lock_exist: exact;
        }
        actions = {
            set_as_primary_action;
            set_as_secondary_action;
            set_as_failure_notification_action;
        }
        size = 4;
    }

    @pragma stage 4
    table update_tail_table {
        actions = {
            update_tail_action;
        }
        const default_action = update_tail_action;
        size = 1;
    }

    @pragma stage 5
    table forward_to_server_table {
        key = {
            hdr.nlk_hdr.lock: ternary;
        }
        actions = {
            forward_to_server_action;
        }
        // const default_action = forward_to_server_action;
        size = 64;
    }

    @pragma stage 5
    table update_ip_array_table {
        actions = {
            update_ip_array_action;
        }
        const default_action = update_ip_array_action;
        size = 1;
    }

    @pragma stage 5
    table update_mode_array_table {
        actions = {
            update_mode_array_action;
        }
        const default_action = update_mode_array_action;
        size = 1;
    }

    @pragma stage 6
    table update_client_id_array_table {
        actions = {
            update_client_id_array_action;
        }
        const default_action = update_client_id_array_action;
        size = 1;
    }

    @pragma stage 6
    table update_tid_array_table {
        actions = {
            update_tid_array_action;
        }
        const default_action = update_tid_array_action;
        size = 1;
    }

    @pragma stage 7
    table update_timestamp_hi_array_table {
        actions = {
            update_timestamp_hi_array_action;
        }
        const default_action = update_timestamp_hi_array_action;
        size = 1;
    }

    @pragma stage 8
    table update_timestamp_lo_array_table {
        actions = {
            update_timestamp_lo_array_action;
        }
        const default_action = update_timestamp_lo_array_action;
        size = 1;
    }

    @pragma stage 8
    table notify_tail_client_table {
        actions = {
            notify_tail_client_action;
        }
        const default_action = notify_tail_client_action;
        size = 1;
    }

    @pragma stage 8
    table switch_direct_grant_table {
        actions = {
            switch_direct_grant_action;
        }
        const default_action = switch_direct_grant_action;
        size = 1;
    }

    @pragma stage 10
    table drop_packet_table {
        actions = {
            drop;
        }
        const default_action = drop;
        size = 1;
    }

    apply {
        // Stage 2: [register_lo: empty_slots_register]
        // ** change the number of empty slots for normal requests and push-back requests
        dec_empty_slots_table.apply();

        // if (ig_md.length_in_server != 0 && hdr.nlk_hdr.op == ACQUIRE_LOCK) {
        // if (ig_md.empty_slots == 0 && hdr.nlk_hdr.op == ACQUIRE_LOCK) {
        if (ig_md.length_in_server != 0 && (hdr.nlk_hdr.op == ACQUIRE_LOCK || hdr.nlk_hdr.op == PUSH_BACK_LOCK)) {
            // * Forward and buffer the request to the server
            // Stage 4
            // ** fix src_port for RSS
            fix_src_port_table.apply();

            set_tag_table.apply();
            // Stage 5
            // ** if the queue is shrinking or is full, forward the request to server
            forward_to_server_table.apply();
        } else {
            // Stage 3: [register_lo: num_exclusive_lock; register_hi: num_shared_lock]
            // ** num_shared_lock ++ or num_exclusive_lock ++; set meta.locked
            acquire_lock_table.apply();

            // Stage 4: [register_lo: tail_register]
            // ** tail ++
            update_tail_table.apply();

            // Stage 5: [register_lo: ip_array_register]
            // ** enqueue(src_ip)
            update_ip_array_table.apply();

            // Stage 5: [register_lo: mode_array_register]
            // ** enqueue(mode)
            update_mode_array_table.apply();

            // Stage 6: [register_lo: client_id_array_register]
            // ** enqueue(client_id)
            update_client_id_array_table.apply();

            // Stage 6: [register_lo: tid_array_register]
            // ** enqueue(tid)
            update_tid_array_table.apply();

            // Stage 7: [register_lo: timestamp_hi_array_register]
            // ** enqueue(timestamp_hi)
            update_timestamp_hi_array_table.apply();

            // Stage 8: [register_lo: timestamp_lo_array_register]
            // ** enqueue(timestamp_lo)
            update_timestamp_lo_array_table.apply();

            if (ig_md.locked == 0) {
                // ** notify the client, grant the lock
                notify_tail_client_table.apply();
                if (hdr.nlk_hdr.op == ACQUIRE_LOCK) {
                    switch_direct_grant_table.apply();
                }
            }
            else {
                // ** drop the packet if locked
                drop_packet_table.apply();
            }
        }
    }
}

control ReleaseLock(
	inout header_t hdr,
    inout metadata_t ig_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {
    // RegisterActions
    RegisterAction<bit<32>, bit<32>, bit<32>>(timestamp_hi_array_register) get_timestamp_hi_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<32>>(timestamp_lo_array_register) get_timestamp_lo_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<32>>(tid_array_register) get_tid_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    RegisterAction<bit<8>, bit<32>, bit<8>>(client_id_array_register) get_client_id_alu = {
        void apply(inout bit<8> value, out bit<8> result) {
            result = value;
        }
    };

    RegisterAction<bit<8>, bit<32>, bit<8>>(mode_array_register) get_mode_alu = {
        void apply(inout bit<8> value, out bit<8> result) {
            result = value;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<32>>(ip_array_register) get_ip_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    RegisterAction<bit<32>, bit<32>, bit<32>>(tail_register) get_tail_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            result = value;
        }
    };

    RegisterAction<pair, bit<32>, bit<1>>(shared_and_exclusive_count_register) update_lock_alu = {
        void apply(inout pair value) {
            if (hdr.nlk_hdr.mode == EXCLUSIVE_LOCK) {
                value.lo = value.lo - 1;
            } else if (hdr.nlk_hdr.mode == SHARED_LOCK) {
                value.hi = value.hi - 1;
            }
        }
    };


    RegisterAction<bit<32>, bit<32>, bit<32>>(head_register) update_head_alu = {
        void apply(inout bit<32> value, out bit<32> result) {
            if (value == ig_md.right) {
                value = ig_md.left;
                result = value;
            } else {
                value = value + 1;
                result = value;
            }
        }
    };

    // Registers
    RegisterAction<pair, bit<32>, bit<32>>(slots_two_sides_register) inc_empty_slots_alu = {
        void apply(inout pair value, out bit<32> result) {
            // if (value.hi > 0) {
            // // empty_slot 
            //     value.lo = value.lo + 1;
            //     result = value.lo;
            // } else {
            //     result = 0;
            // }
            result = value.lo;
            // if (value.lo < ig_md.size_of_queue) {
            if (value.lo <= ig_md.size_of_queue) {
                // queue not empty, switch pop
                value.lo = value.lo + 1;
            }
            // } else {
            //     // queue empty, forward to server
            //     // not update length_in_server here, update when server push back
            //     // result = value.lo;
            // }   
        }
    };

    // Actions
    action get_recirc_info_action() {
        ig_md.tail = hdr.recirculate_hdr.cur_tail;
        ig_md.head = hdr.recirculate_hdr.cur_head;
        ig_md.recirc_flag = hdr.nlk_hdr.recirc_flag;
        ig_md.dequeued_mode = hdr.recirculate_hdr.dequeued_mode;
    }

    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    action fix_src_port_action(bit<16> fix_port) {
        hdr.ethernet.srcAddr = (bit<48>)hdr.ipv4.srcAddr; // TODO
        hdr.udp.srcPort = fix_port;
    }

    action set_as_primary_action() {
        hdr.ethernet.dstAddr = PRIMARY_BACKUP;
    }

    action set_as_secondary_action() {
        hdr.ethernet.dstAddr = SECONDARY_BACKUP;
    }

    action set_as_failure_notification_action() {
        hdr.ethernet.dstAddr = FAILURE_NOTIFICATION;
    }

    action forward_to_server_action(ipv4_addr_t server_ip) {
        hdr.ipv4.srcAddr = server_ip;
        hdr.ipv4.dstAddr = server_ip;
        hdr.nlk_hdr.empty_slots = ig_md.empty_slots_before_pop;
        hdr.nlk_hdr.head = ig_md.head;
        hdr.nlk_hdr.tail = ig_md.tail;
    }

    action notify_head_client_action() {
        hdr.ipv4.dstAddr = ig_md.ip_address;
        // hdr.nlk_hdr.empty_slots = ig_md.empty_slots_before_pop;
        // hdr.nlk_hdr.head = ig_md.head;
        // hdr.nlk_hdr.tail = ig_md.tail;
    }

    action set_egress_2(PortId_t egress_spec) {
        // ig_intr_tm_md.ucast_egress_port = egress_spec;
        ig_md.routed = 1;
        // ig_md.ing_mir_ses = INVALID_MIRROR_ID;
        // mirror
        // hdr.bridged_md.setValid();
        // hdr.bridged_md.pkt_type = PKT_TYPE_NORMAL;
        // ig_intr_dprsr_md.mirror_type = MIRROR_TYPE_NORMAL;
        // resubmit
        // ig_intr_dprsr_md.resubmit_type = NO_RESUBMIT;

        ig_intr_tm_md.bypass_egress = 1w1;
    }

    action i2e_mirror_action(MirrorId_t mirror_id) {
        // notify the client that has obtained the lock
        ig_md.clone_md = 1;
        ig_intr_dprsr_md.mirror_type = MIRROR_TYPE_I2E;
        ig_md.ing_mir_ses = mirror_id;
        ig_md.pkt_type = PKT_TYPE_MIRROR;
    }

    action metahead_plus_1_action() {
        ig_md.head = ig_md.left;
    }

    action metahead_plus_2_action() {
        ig_md.head = ig_md.head + 1;
    }

    action resubmit_action() {
        ig_md.recirced = 1;
        hdr.recirculate_hdr.cur_tail = ig_md.tail;
        hdr.recirculate_hdr.cur_head = ig_md.head;
        ig_intr_tm_md.ucast_egress_port = 68; // Send to recirculate egress port
        ig_intr_tm_md.bypass_egress = 1w1;
    }

    action mark_to_resubmit_action() {
        hdr.nlk_hdr.recirc_flag = 1;
        hdr.recirculate_hdr.setValid();
        // hdr.recirculate_hdr.cur_tail = ig_md.tail;
        // hdr.recirculate_hdr.cur_head = ig_md.head;
        hdr.recirculate_hdr.dequeued_mode = ig_md.mode;
        ig_md.do_resubmit = 1;
    }

    action mark_to_resubmit_2_action() {
        hdr.nlk_hdr.recirc_flag = 2;
        // hdr.recirculate_hdr.cur_tail = ig_md.tail;
        // hdr.recirculate_hdr.cur_head = ig_md.head;
        ig_md.do_resubmit = 1;
    }

    action inc_empty_slots_action() {
        ig_md.empty_slots_before_pop = inc_empty_slots_alu.execute(ig_md.lock_id);
    }

    action update_head_action() {
        ig_md.head = update_head_alu.execute(ig_md.lock_id);
    }

    action update_lock_action() {
        update_lock_alu.execute(ig_md.lock_id);
    }

    action get_timestamp_hi_action() {
        // ig_md.timestamp_hi = get_timestamp_hi_alu.execute(ig_md.head);
    }

    action get_timestamp_lo_action() {
        // ig_md.timestamp_lo = get_timestamp_lo_alu.execute(ig_md.head);
    }

    action get_tid_action() {
        ig_md.tid = get_tid_alu.execute(ig_md.head);
    }

    action get_client_id_action() {
        ig_md.client_id = get_client_id_alu.execute(ig_md.head);
    }

    action get_mode_action() {
        ig_md.mode = get_mode_alu.execute(ig_md.head);
    }

    action get_ip_action() {
        ig_md.ip_address = get_ip_alu.execute(ig_md.head);
        ig_md.timestamp_hi = ig_md.tail;
        ig_md.timestamp_lo = ig_md.head;
    }

    action get_tail_action() {
        ig_md.tail = get_tail_alu.execute(ig_md.lock_id);
    }

    action nop() {}
 
    // Tables

    @pragma stage 3
    table get_recirc_info_table {
        actions = {
            get_recirc_info_action;
        }
        const default_action = get_recirc_info_action;
        size = 1;
    }

   @pragma stage 3
    table inc_empty_slots_table {
        actions = {
            inc_empty_slots_action;
        }
        const default_action = inc_empty_slots_action;
        size = 1;
    }

    @pragma stage 4
    table update_head_table {
        actions = {
            update_head_action;
        }
        const default_action = update_head_action;
        size = 1;
    }

    @pragma stage 4
    table update_lock_table {
        actions = {
            update_lock_action;
        }
        const default_action = update_lock_action;
        size = 1;
    }

    @pragma stage 4
    table get_tail_table {
        actions = {
            get_tail_action;
        }
        const default_action = get_tail_action;
        size = 1;
    }

    @pragma stage 4
    table fix_src_port_table {
        key = {
            hdr.nlk_hdr.lock: ternary;
        }
        actions = {
            fix_src_port_action;
        }
        size = 256;
    }

    @pragma stage 4
    table set_tag_table {
        key = {
            ig_md.failure_status: exact;
            ig_md.lock_exist: exact;
        }
        actions = {
            set_as_primary_action;
            set_as_secondary_action;
            set_as_failure_notification_action;
        }
        size = 4;
    }
 
    @pragma stage 5
    table get_mode_table {
        actions = {
            get_mode_action;
        }
        const default_action = get_mode_action;
        size = 1;
    }

    @pragma stage 5
    table get_ip_table {
        actions = {
            get_ip_action;
        }
        const default_action = get_ip_action;
        size = 1;
    }

    @pragma stage 5
    table forward_to_server_table {
        key = {
            hdr.nlk_hdr.lock: ternary;
        }
        actions = {
            forward_to_server_action;
        }
        // const default_action = forward_to_server_action;
        size = 64;
    }

    @pragma stage 6
    table notify_head_client_table {
        actions = {
            notify_head_client_action;
        }
        const default_action = notify_head_client_action;
        size = 1;
    }
 
    @pragma stage 6
    table get_tid_table {
        actions = {
            get_tid_action;
        }
        const default_action = get_tid_action;
        size = 1;
    }

    @pragma stage 6
    table get_client_id_table {
        actions = {
            get_client_id_action;
        }
        const default_action = get_client_id_action;
        size = 1;
    }

    @pragma stage 7
    table get_timestamp_hi_table {
        actions = {
            get_timestamp_hi_action;
        }
        const default_action = get_timestamp_hi_action;
        size = 1;
    }

    @pragma stage 8
    table get_timestamp_lo_table {
        actions = {
            get_timestamp_lo_action;
        }
        const default_action = get_timestamp_lo_action;
        size = 1;
    }

   @pragma stage 8
    table ipv4_route_2 {
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            set_egress_2;
            drop;
        }
        size = 128;
        const default_action = drop;
    }

    @pragma stage 8
    table i2e_mirror_table {
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            i2e_mirror_action;
        }
        size = 128;
    }

    @pragma stage 8
    table metahead_plus_1_table {
        actions = {
            metahead_plus_1_action;
        }
        const default_action = metahead_plus_1_action;
        size = 1;
    }

    @pragma stage 8
    table metahead_plus_2_table {
        actions = {
            metahead_plus_2_action;
        }
        const default_action = metahead_plus_2_action;
        size = 1;
    }

    @pragma stage 9
    table mark_to_resubmit_table {
        actions = {
            mark_to_resubmit_action;
        }
        const default_action = mark_to_resubmit_action;
        size = 1;
    }

    @pragma stage 9
    table mark_to_resubmit_2_table {
        actions = {
            mark_to_resubmit_2_action;
        }
        const default_action = mark_to_resubmit_2_action;
        size = 1;
    }

    @pragma stage 10
    table drop_packet_table {
        actions = {
            drop;
        }
        const default_action = drop;
        size = 1;
    }

    table resubmit_table {
        actions = {
            resubmit_action;
        }
        const default_action = resubmit_action;
        size = 1;
    }

    apply {
        if (hdr.nlk_hdr.recirc_flag == 0) {
            // ig_md.recirc_flag = 0; // TODO
            // Stage 2: [register_lo: empty_slots_register]
            // ** num_of_empty_slots ++
            inc_empty_slots_table.apply();
            // Stage 3: [register_lo: head_register]
            // ** head ++
            // if (ig_md.empty_slots_before_pop != ig_md.size_of_queue) {
            //     update_head_table.apply();
            //     // Stage 3: [register_lo: num_exclusive_lock; register_hi: num_shared_lock]
            //     // ** num_shared_lock -- or num_exclusive_lock --
            //     update_lock_table.apply();
            // }   
            update_head_table.apply();
            update_lock_table.apply();
        } else {
            // Stage 2
            // ** it is a resubmit packet
            get_recirc_info_table.apply();
        }

        if (ig_md.empty_slots_before_pop == ig_md.size_of_queue && hdr.nlk_hdr.recirc_flag == 0) {
            // * If the switch queue is empty, get some back from the server queue
            // Stage 4
            // ** fix src_port for RSS
            fix_src_port_table.apply();

            set_tag_table.apply();
            // Stage 5
            // ** forward release request to server if the queue is empty
            forward_to_server_table.apply();

        } else {
            // Stage 4: [register_lo: tail_register]
            // ** get tail
            if (hdr.nlk_hdr.recirc_flag == 0) {
                get_tail_table.apply();
            }

            // Stage 5: [register_lo: ip_array_register]
            // ** get head node inf (src_ip)
            get_ip_table.apply();

            // Stage 5: [register_lo: mode_array_register]
            // ** get head node inf (mode)
            get_mode_table.apply();

            // Stage 6: [register_lo: client_id_array_register]
            // ** get head node inf (client_id)
            get_client_id_table.apply();

            // Stage 6: [register_lo: tid_array_register]
            // ** get head node inf (tid)
            get_tid_table.apply();

            // Stage 6
            // ** change the ip_dst
            notify_head_client_table.apply();

            // Stage 7: [register_lo: timestamp_hi_array_register]
            // ** get timestamp_hi
            get_timestamp_hi_table.apply();

            // Stage 8: [register_lo: timestamp_lo_array_register]
            // ** get timestamp_lo
            get_timestamp_lo_table.apply();

            if ((ig_md.recirc_flag == 1 && (ig_md.dequeued_mode == EXCLUSIVE_LOCK || ig_md.mode == EXCLUSIVE_LOCK)) 
                || (ig_md.recirc_flag == 2 && ig_md.mode == SHARED_LOCK)) {
            // if ((ig_md.recirc_flag == 1 && ig_md.dequeued_mode == EXCLUSIVE_LOCK) || (ig_md.mode == EXCLUSIVE_LOCK) 
            //     || ((ig_md.recirc_flag == 2) && (ig_md.mode == SHARED_LOCK))) {
                // Stage 7
                // ** modify the ipv4 address for the packet
                // ipv4_route_2.apply();

                // Stage 7
                // ** mirror the packet, one to notify the client, one to go through resubmit procedure again
                i2e_mirror_table.apply();
            }

            if ((ig_md.recirc_flag == 1 && (ig_md.dequeued_mode == EXCLUSIVE_LOCK || ig_md.mode == EXCLUSIVE_LOCK)) 
                || (ig_md.recirc_flag == 2 && ig_md.mode == SHARED_LOCK) 
                || ig_md.recirc_flag == 0) {
                if (ig_md.head != ig_md.tail) {
                    if (ig_md.recirc_flag == 0) {
                        mark_to_resubmit_table.apply();

                    } else if ((ig_md.recirc_flag == 1 && ig_md.mode == SHARED_LOCK) || (ig_md.recirc_flag == 2)) {
                        // ** ESSS
                        mark_to_resubmit_2_table.apply();
                    }
                }

                // ** meta.head ++ (point to the next item)
                if (ig_md.head == ig_md.right) {
                    metahead_plus_1_table.apply();
                } else {
                    metahead_plus_2_table.apply();
                }

                if (ig_md.do_resubmit == 1) {
                    resubmit_table.apply();
                }
            }

            // ** drop the original packet
            if (ig_md.do_resubmit == 0) {
                drop_packet_table.apply();
            }
        }
    }
}

control SwitchIngress(
	inout header_t hdr,
    inout metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {
    Decode() decode;
    AcquireLock() acquire_lock;
    ReleaseLock() release_lock;

    action set_egress(PortId_t egress_spec) {
        ig_intr_tm_md.ucast_egress_port = egress_spec;
        // mirror
        // hdr.bridged_md.setValid();
        // hdr.bridged_md.pkt_type = PKT_TYPE_NORMAL;
        // ig_intr_dprsr_md.mirror_type = MIRROR_TYPE_NORMAL;
        // ig_md.ing_mir_ses = INVALID_MIRROR_ID;
        ig_intr_tm_md.bypass_egress = 1w1;
        ig_md.routed = 1;
    }

    action check_lock_exist_action (bit<32> index) {
        ig_md.lock_exist = 1;
        ig_md.lock_id = index;
    }

    action fix_src_port_action(bit<16> fix_port) {
        hdr.ethernet.srcAddr = (bit<48>)hdr.ipv4.srcAddr; // TODO
        hdr.udp.srcPort = fix_port;
    }

    action set_as_primary_action() {
        hdr.ethernet.dstAddr = PRIMARY_BACKUP;
    }

    action set_as_secondary_action() {
        hdr.ethernet.dstAddr = SECONDARY_BACKUP;
    }

    action set_as_failure_notification_action() {
        hdr.ethernet.dstAddr = FAILURE_NOTIFICATION;
    }

    action forward_to_server_action(ipv4_addr_t server_ip) {
        hdr.ipv4.srcAddr = server_ip;
        hdr.ipv4.dstAddr = server_ip;
    }

    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    // Tables
    @pragma stage 0
    table check_lock_exist_table {
        key = {
            hdr.nlk_hdr.lock: exact;
        }
        actions = {
            check_lock_exist_action;
        }
        size = NUM_LOCKS;
    }

    @pragma stage 4
    table fix_src_port_table {
        key = {
            hdr.nlk_hdr.lock: ternary;
        }
        actions = {
            fix_src_port_action;
        }
        size = 256;
    }

    @pragma stage 4
    table set_tag_table {
        key = {
            ig_md.failure_status: exact;
            ig_md.lock_exist: exact;
        }
        actions = {
            set_as_primary_action;
            set_as_secondary_action;
            set_as_failure_notification_action;
        }
        size = 4;
    }

    @pragma stage 5
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

    @pragma stage 11
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

    table ipv4_route_table_2 {
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            set_egress;
            drop;
        }
        size = 16;
        const default_action = drop;
    }

    action do_recirculate_2() {
        hdr.nlk_hdr.recirc_flag = 2;
        hdr.recirculate_hdr.dequeued_mode = hdr.recirculate_hdr.dequeued_mode + 1;
        ig_md.recirced = 1;
        ig_intr_tm_md.ucast_egress_port = 68;
        ig_intr_tm_md.bypass_egress = 1w1;
    }

    table recirculate_table_2 {
        actions = {
            do_recirculate_2;
        }
        const default_action = do_recirculate_2;
        size = 1;
    }

    action do_recirculate() {
        hdr.nlk_hdr.recirc_flag = 1;
        hdr.recirculate_hdr.setValid();
        hdr.recirculate_hdr.dequeued_mode = 1;
        ig_md.recirced = 1;
        ig_intr_tm_md.ucast_egress_port = 68;
        ig_intr_tm_md.bypass_egress = 1w1;
    }

    table recirculate_table {
        actions = {
            do_recirculate;
        }
        const default_action = do_recirculate;
        size = 1;
    }

    action i2e_clone_action(MirrorId_t mirror_id) {
        ig_md.clone_md = 1;
        ig_intr_dprsr_md.mirror_type = MIRROR_TYPE_I2E;
        ig_md.ing_mir_ses = mirror_id;
        ig_md.pkt_type = PKT_TYPE_MIRROR;
    }

    table i2e_clone_table {
        key = {
            hdr.ipv4.dstAddr: exact;
        }
        actions = {
            i2e_clone_action;
            drop;
        }
        const default_action = drop;
        size = 16;
    }

    apply {
        if (hdr.nlk_hdr.isValid() && hdr.udp.dstPort == UDP_WARMUP_DST_PORT) {
            if (hdr.recirculate_hdr.isValid()) {
                if (hdr.recirculate_hdr.dequeued_mode < 2) {
                    recirculate_table_2.apply();
                    i2e_clone_table.apply();
                } else {
                    ipv4_route_table_2.apply();
                }
            } else {
                recirculate_table.apply();
            }
        } else {
            if (hdr.nlk_hdr.isValid() && (hdr.nlk_hdr.op == ACQUIRE_LOCK || hdr.nlk_hdr.op == PUSH_BACK_LOCK || hdr.nlk_hdr.op == RELEASE_LOCK)) {
                check_lock_exist_table.apply();
                if (ig_md.lock_exist == 1) {
                    /* The switch is working, and is responsible for this lock */
                    decode.apply(hdr, ig_md);
                    if (hdr.nlk_hdr.op == ACQUIRE_LOCK || hdr.nlk_hdr.op == PUSH_BACK_LOCK) {
                        /* Handle normal lock requests and requests pushed back from the server */
                        acquire_lock.apply(hdr, ig_md, ig_intr_dprsr_md);
                    } else if (hdr.nlk_hdr.op == RELEASE_LOCK) {
                        release_lock.apply(hdr, ig_md, ig_intr_dprsr_md, ig_intr_tm_md);
                    }
                } else {
                    fix_src_port_table.apply();
                    set_tag_table.apply();
                    forward_to_server_table.apply();
                }
            }
        }
        if (hdr.nlk_hdr.isValid() && ig_md.routed == 0 && ig_md.recirced == 0) {
            ipv4_route_table.apply();
        }
    }
}