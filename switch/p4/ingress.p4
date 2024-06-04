
#include "lock.p4"
#include "counter.p4"

control IngressPipe(
	inout header_t hdr,
    inout metadata_t ig_md,
    in ingress_intrinsic_metadata_t ig_intr_md,
    in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
    inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
    inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {

    Register<bit<1>, lid_t>(SLICE_SIZE * SLICE_NUM, 0) lock_free_mode_array;
    Register<bit<1>, lid_t>(SLICE_SIZE * SLICE_NUM, 0) lock_rw_mode_array;

    RegisterAction<bit<1>, lid_t, bit<1>>(lock_free_mode_array) acquire = {
        void apply(inout bit<1> value, out bit<1> state) {
            state = value;
            value = LOCK_ACQUIRED;
        }
    };

    RegisterAction<bit<1>, lid_t, bit<1>>(lock_free_mode_array) release = {
        void apply(inout bit<1> value, out bit<1> state) {
            state = value;
            value = LOCK_FREE;
        }
    };

    RegisterAction<bit<1>, lid_t, bit<1>>(lock_rw_mode_array) set_shared = {
        void apply(inout bit<1> value, out bit<1> state) {
            state = value;
            value = LOCK_SHARED;
        }
    };

    RegisterAction<bit<1>, lid_t, bit<1>>(lock_rw_mode_array) set_excl = {
        void apply(inout bit<1> value, out bit<1> state) {
            state = value;
            value = LOCK_EXCL;
        }
    };

    RegisterAction<bit<1>, lid_t, bit<1>>(lock_rw_mode_array) get_mode = {
        void apply(inout bit<1> value, out bit<1> state) {
            state = value;
        }
    };

    action drop() {
        ig_intr_dprsr_md.drop_ctl = 0x1;
    }

    action nop() {}

	action eth_forward(PortId_t port) {
        ig_intr_tm_md.ucast_egress_port = port;
        ig_intr_dprsr_md.drop_ctl = 0x0;
	}

    action forward_to_host(PortId_t port) {
        ig_intr_tm_md.ucast_egress_port = port;
        ig_intr_dprsr_md.drop_ctl = 0x0;
    }

    action lock_shared() {
        ig_md.lock_rw_mode = set_shared.execute(hdr.lock.id);
    }

    action lock_excl() {
        ig_md.lock_rw_mode = set_excl.execute(hdr.lock.id);
    }

    action lock_mode_get() {
        ig_md.lock_rw_mode = get_mode.execute(hdr.lock.id);
    }

    action acquire_lock() {
        ig_md.lock_free_mode = acquire.execute(hdr.lock.id);
    }

    action release_lock() {
        ig_md.lock_free_mode = release.execute(hdr.lock.id);
    }

    table eth_fallback {
        key = {
            hdr.ethernet.dst_mac: exact;
        }

        actions = {
            eth_forward;
            @defaultonly drop;
        }

        const default_action = drop;
        size = 32;
    }

    table rw_table {
        key = {
            hdr.lock.type: exact;
            ig_md.lock_free_mode: exact;
            hdr.lock.mode: exact;
        }

        actions = {
            lock_shared;
            lock_excl;
            lock_mode_get;
            nop;
        }

        const entries = {
            (TRANSFER, LOCK_ACQUIRED, LOCK_SHARED): lock_shared();
            (TRANSFER, LOCK_ACQUIRED, LOCK_EXCL): lock_excl();
            (FREE, LOCK_ACQUIRED, LOCK_SHARED): lock_shared();
            (FREE, LOCK_ACQUIRED, LOCK_EXCL): lock_excl();

            (ACQUIRE, LOCK_FREE, LOCK_SHARED): lock_shared();
            (ACQUIRE, LOCK_FREE, LOCK_EXCL): lock_excl();
            (ACQUIRE, LOCK_ACQUIRED, LOCK_SHARED): lock_mode_get();
            (ACQUIRE, LOCK_ACQUIRED, LOCK_EXCL): lock_mode_get();
        }

        const default_action = nop;
        size = 16;
    }

    table acquire_table {
        actions = {
            acquire_lock;
        }
        const default_action = acquire_lock;
        size = 1;
    }

    table release_table {
        actions = {
            release_lock;
        }
        const default_action = release_lock;
        size = 1;
    }

    apply {

        if (hdr.lock.isValid()) {

            // Initialize ig_md.
            ig_md.dest2 = 0;
            ig_md.agent_changed = 0;
            ig_md.lock_out_of_range = 0;
            hdr.lock.multicasted = 0;
            hdr.lock.granted = 0;

            // Some arrays splitted into multiple stages, because 
            // the memory capacity of a single stage is not sufficient
            // for supporting million-scale metadata.
            // 
            // We calculate the index inside of each stage here.
            ig_md.lock_index = hdr.lock.id;
            ig_md.lock_index[31:SLICE_SIZE_POW2] = 0;

            if (hdr.lock.id[31:SLICE_SIZE_POW2] == 0) {
                CounterTable_1.apply(hdr, ig_md);
            } else if (hdr.lock.id[31:SLICE_SIZE_POW2] == 1) {
                CounterTable_2.apply(hdr, ig_md);
            } else if (hdr.lock.id[31:SLICE_SIZE_POW2] == 2) {
                CounterTable_3.apply(hdr, ig_md);
            } else {
                ig_md.lock_out_of_range = 1;
            }

            // If the lock is out of range, agent is specified as the
            // default lock manager, just forward.
            if (ig_md.lock_out_of_range == 1) {
                if (hdr.lock.type == GRANT_WO_AGENT) {
                    hdr.udp.dst_port = UDP_PORT_CLIENT;
                }

            // If the counter in transfer/release packet does not match with
            // the counter on the switch, there must be packets in the network.
            // In this case, we push the packet back to the original agent.
            } else if ((hdr.lock.type == TRANSFER || hdr.lock.type == FREE) && 
                hdr.lock.old_mode == LOCK_SHARED && ig_md.agent_changed == 0) {

                // Do not need to change the agent here, skip the
                // rest of the lock operation workflow.
                // hdr.lock.agent = hdr.lock.agent;
           
            // We forward grant packets from the agent without processing.
            } else if (hdr.lock.type == GRANT_WO_AGENT) {
                hdr.lock.agent = hdr.lock.machine_id;

            } else {

                // Access the free RA.
                if (hdr.lock.type != FREE) {
                    acquire_table.apply();
                } else {
                    release_table.apply();
                }

                // Access the R/W RA.
                rw_table.apply();

                // We might want to update the lock agent identity later,
                // so we set this in an earlier stage.
                ig_md.lock_agent = hdr.lock.machine_id;

                // Execute the lock operation.
                if (hdr.lock.id[31:SLICE_SIZE_POW2] == 0) {
                    LockOperation_1.apply(hdr, ig_md);
                } else if (hdr.lock.id[31:SLICE_SIZE_POW2] == 1) {
                    LockOperation_2.apply(hdr, ig_md);
                } else {
                    LockOperation_3.apply(hdr, ig_md);
                }
            }

            // Forward to the destination port indicated by host ID.
            // 
            // To save stages used for condition branches, we do not
            // check whether we should unicast or multicast.
            // Instead, we do not set the mgrp 0 and 128, so when the
            // lock.agent or/and dest2 is 0, the replicated 
            // packet will be dropped by the traffic manager.
            ig_intr_tm_md.mcast_grp_a = (bit<16>)hdr.lock.agent;
            ig_intr_tm_md.mcast_grp_b = ig_md.dest2 + 16w128;

        } else if (hdr.ethernet.isValid()) {
            eth_fallback.apply();
		}
    }
}