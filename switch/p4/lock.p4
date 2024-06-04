control LockOperation_1(
	inout header_t hdr,
	inout metadata_t ig_md) {

    Register<host_t, lid_t>(SLICE_SIZE, 0) lock_agent_array_1;

    RegisterAction<host_t, lid_t, host_t>(lock_agent_array_1) get_lock_agent = {
        void apply(inout host_t value, out host_t agent) {
            agent = value;
        }
    };

    RegisterAction<host_t, lid_t, host_t>(lock_agent_array_1) set_lock_agent = {
        void apply(inout host_t value) {
            value = ig_md.lock_agent;
        }
    };

    action new_agent() {
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = hdr.lock.machine_id;
        hdr.lock.type = GRANT_W_AGENT;
        hdr.udp.dst_port = UDP_PORT_CLIENT;
        hdr.lock.transferred = 0;
    }

    action mcast_to_agent() {
        ig_md.dest2 = (bit<16>)hdr.lock.machine_id;
        hdr.lock.multicasted = 1;
        hdr.lock.agent = get_lock_agent.execute(ig_md.lock_index);
        hdr.lock.type = GRANT_WO_AGENT;
    }

    action fwd_to_agent() {
        hdr.lock.agent = get_lock_agent.execute(ig_md.lock_index);
        hdr.udp.dst_port = UDP_PORT_SERVER;
    }

    action transfer_agent() {
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = hdr.lock.machine_id;
        hdr.lock.type = GRANT_W_AGENT;
        hdr.udp.dst_port = UDP_PORT_CLIENT;
        hdr.lock.transferred = 1;
    }

    action reset_agent() {
        ig_md.lock_agent = 0;
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = 0;
    }

    action nop() {}

    table lock_operation_1 {
        key = {
            hdr.lock.type: exact;
            hdr.lock.mode: exact;
            ig_md.lock_free_mode: exact;
            ig_md.lock_rw_mode: exact;
        }

        actions = {
            new_agent;
            mcast_to_agent;
            fwd_to_agent;
            transfer_agent;
            reset_agent;
            nop;
        }

        const entries = {
            (ACQUIRE, LOCK_SHARED, LOCK_FREE, 0): new_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_FREE, 1): new_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_FREE, 0): new_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_FREE, 1): new_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): mcast_to_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): fwd_to_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();

            (RELEASE, 0, LOCK_ACQUIRED, LOCK_SHARED): fwd_to_agent();
            (RELEASE, 0, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();

            (TRANSFER, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): transfer_agent();
            (TRANSFER, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): transfer_agent();
            (TRANSFER, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): transfer_agent();
            (TRANSFER, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): transfer_agent();

            (FREE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): reset_agent();
            (FREE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): reset_agent();
            (FREE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): reset_agent();
            (FREE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): reset_agent();
        }

        const default_action = nop;
        size = 32;
    }

    apply {
        lock_operation_1.apply();
    }
}

control LockOperation_2(
	inout header_t hdr,
	inout metadata_t ig_md) {

    Register<host_t, lid_t>(SLICE_SIZE, 0) lock_agent_array_2;

    RegisterAction<host_t, lid_t, host_t>(lock_agent_array_2) get_lock_agent = {
        void apply(inout host_t value, out host_t agent) {
            agent = value;
        }
    };

    RegisterAction<host_t, lid_t, host_t>(lock_agent_array_2) set_lock_agent = {
        void apply(inout host_t value) {
            value = ig_md.lock_agent;
        }
    };

    action new_agent() {
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = hdr.lock.machine_id;
        hdr.lock.type = GRANT_W_AGENT;
        hdr.udp.dst_port = UDP_PORT_CLIENT;
        hdr.lock.transferred = 0;
    }

    action mcast_to_agent() {
        ig_md.dest2 = (bit<16>)hdr.lock.machine_id;
        hdr.lock.multicasted = 1;
        hdr.lock.agent = get_lock_agent.execute(ig_md.lock_index);
        hdr.lock.type = GRANT_WO_AGENT;
    }

    action fwd_to_agent() {
        hdr.lock.agent = get_lock_agent.execute(ig_md.lock_index);
        hdr.udp.dst_port = UDP_PORT_SERVER;
    }

    action transfer_agent() {
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = hdr.lock.machine_id;
        hdr.lock.type = GRANT_W_AGENT;
        hdr.udp.dst_port = UDP_PORT_CLIENT;
        hdr.lock.transferred = 1;
    }

    action reset_agent() {
        ig_md.lock_agent = 0;
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = 0;
    }

    action nop() {}

    table lock_operation_2 {
        key = {
            hdr.lock.type: exact;
            hdr.lock.mode: exact;
            ig_md.lock_free_mode: exact;
            ig_md.lock_rw_mode: exact;
        }

        actions = {
            new_agent;
            mcast_to_agent;
            fwd_to_agent;
            transfer_agent;
            reset_agent;
            nop;
        }

        const entries = {
            (ACQUIRE, LOCK_SHARED, LOCK_FREE, 0): new_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_FREE, 1): new_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_FREE, 0): new_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_FREE, 1): new_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): mcast_to_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): fwd_to_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();

            (RELEASE, 0, LOCK_ACQUIRED, LOCK_SHARED): fwd_to_agent();
            (RELEASE, 0, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();

            (TRANSFER, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): transfer_agent();
            (TRANSFER, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): transfer_agent();
            (TRANSFER, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): transfer_agent();
            (TRANSFER, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): transfer_agent();

            (FREE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): reset_agent();
            (FREE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): reset_agent();
            (FREE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): reset_agent();
            (FREE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): reset_agent();
        }

        const default_action = nop;
        size = 32;
    }

    apply {
        lock_operation_2.apply();
    }
}

control LockOperation_3(
	inout header_t hdr,
	inout metadata_t ig_md) {

    Register<host_t, lid_t>(SLICE_SIZE, 0) lock_agent_array_3;

    RegisterAction<host_t, lid_t, host_t>(lock_agent_array_3) get_lock_agent = {
        void apply(inout host_t value, out host_t agent) {
            agent = value;
        }
    };

    RegisterAction<host_t, lid_t, host_t>(lock_agent_array_3) set_lock_agent = {
        void apply(inout host_t value) {
            value = ig_md.lock_agent;
        }
    };

    action new_agent() {
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = hdr.lock.machine_id;
        hdr.lock.type = GRANT_W_AGENT;
        hdr.udp.dst_port = UDP_PORT_CLIENT;
        hdr.lock.transferred = 0;
    }

    action mcast_to_agent() {
        ig_md.dest2 = (bit<16>)hdr.lock.machine_id;
        hdr.lock.multicasted = 1;
        hdr.lock.agent = get_lock_agent.execute(ig_md.lock_index);
        hdr.lock.type = GRANT_WO_AGENT;
    }

    action fwd_to_agent() {
        hdr.lock.agent = get_lock_agent.execute(ig_md.lock_index);
        hdr.udp.dst_port = UDP_PORT_SERVER;
    }

    action transfer_agent() {
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = hdr.lock.machine_id;
        hdr.lock.type = GRANT_W_AGENT;
        hdr.udp.dst_port = UDP_PORT_CLIENT;
        hdr.lock.transferred = 1;
    }

    action reset_agent() {
        ig_md.lock_agent = 0;
        set_lock_agent.execute(ig_md.lock_index);
        hdr.lock.agent = 0;
    }

    action nop() {}

    table lock_operation_3 {
        key = {
            hdr.lock.type: exact;
            hdr.lock.mode: exact;
            ig_md.lock_free_mode: exact;
            ig_md.lock_rw_mode: exact;
        }

        actions = {
            new_agent;
            mcast_to_agent;
            fwd_to_agent;
            transfer_agent;
            reset_agent;
            nop;
        }

        const entries = {
            (ACQUIRE, LOCK_SHARED, LOCK_FREE, 0): new_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_FREE, 1): new_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_FREE, 0): new_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_FREE, 1): new_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): mcast_to_agent();
            (ACQUIRE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): fwd_to_agent();
            (ACQUIRE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();

            (RELEASE, 0, LOCK_ACQUIRED, LOCK_SHARED): fwd_to_agent();
            (RELEASE, 0, LOCK_ACQUIRED, LOCK_EXCL): fwd_to_agent();

            (TRANSFER, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): transfer_agent();
            (TRANSFER, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): transfer_agent();
            (TRANSFER, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): transfer_agent();
            (TRANSFER, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): transfer_agent();

            (FREE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_SHARED): reset_agent();
            (FREE, LOCK_SHARED, LOCK_ACQUIRED, LOCK_EXCL): reset_agent();
            (FREE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_SHARED): reset_agent();
            (FREE, LOCK_EXCL, LOCK_ACQUIRED, LOCK_EXCL): reset_agent();
        }

        const default_action = nop;
        size = 32;
    }

    apply {
        lock_operation_3.apply();
    }
}
