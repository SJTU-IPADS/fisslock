control CounterTable_1(
	inout header_t hdr,
	inout metadata_t ig_md) {

    Register<bit<8>, lid_t>(SLICE_SIZE, 0) notification_cnt_1;

    RegisterAction<bit<8>, lid_t, bit<8>>(notification_cnt_1) count_ncnt = {
        void apply(inout bit<8> value) {
            value = value + 1;
        }
    };

    RegisterAction<bit<8>, lid_t, bit<8>>(notification_cnt_1) reset_ncnt = {
        void apply(inout bit<8> value) {
            value = 0;
        }
    };

    RegisterAction<bit<8>, lid_t, bit<1>>(notification_cnt_1) cmp_ncnt = {
        void apply(inout bit<8> value, out bit<1> flag) {
            if (value == hdr.lock.ncnt) {
                value = 0;
                flag = 1;
            } else {
                flag = 0;
            }
        }
    };


    action get_notification_cnt() {
        ig_md.agent_changed = cmp_ncnt.execute(ig_md.lock_index);
    }

    action new_notification() {
        count_ncnt.execute(ig_md.lock_index);
    }

    action reset_notification_cnt() {
        reset_ncnt.execute(ig_md.lock_index);
    }

    action nop() {}

    table counter_table_1 {
        key = {
            hdr.lock.type: exact;
            hdr.lock.mode: exact;
            hdr.lock.old_mode: exact;
        }

        actions = {
            get_notification_cnt;
            reset_notification_cnt;
            new_notification;
            nop;
        }
        
        const entries = {
            // Type Mode Old-mode
            (ACQUIRE, LOCK_SHARED, 0): new_notification();
            (ACQUIRE, LOCK_SHARED, 1): new_notification();

            (TRANSFER, LOCK_SHARED, LOCK_SHARED): get_notification_cnt();
            (TRANSFER, LOCK_SHARED, LOCK_EXCL): reset_notification_cnt();
            (TRANSFER, LOCK_EXCL, LOCK_SHARED): get_notification_cnt();
            (TRANSFER, LOCK_EXCL, LOCK_EXCL): reset_notification_cnt();

            (FREE, LOCK_SHARED, LOCK_SHARED): get_notification_cnt();
            (FREE, LOCK_SHARED, LOCK_EXCL): reset_notification_cnt();
            (FREE, LOCK_EXCL, LOCK_SHARED): get_notification_cnt();
            (FREE, LOCK_EXCL, LOCK_EXCL): reset_notification_cnt();
        }

        const default_action = nop;
        size = 16;
    }

    apply {
        counter_table_1.apply();
    }
}

control CounterTable_2(
	inout header_t hdr,
	inout metadata_t ig_md) {

    Register<bit<8>, lid_t>(SLICE_SIZE, 0) notification_cnt_2;

    RegisterAction<bit<8>, lid_t, bit<8>>(notification_cnt_2) count_ncnt = {
        void apply(inout bit<8> value) {
            value = value + 1;
        }
    };

    RegisterAction<bit<8>, lid_t, bit<8>>(notification_cnt_2) reset_ncnt = {
        void apply(inout bit<8> value) {
            value = 0;
        }
    };

    RegisterAction<bit<8>, lid_t, bit<1>>(notification_cnt_2) cmp_ncnt = {
        void apply(inout bit<8> value, out bit<1> flag) {
            if (value == hdr.lock.ncnt) {
                value = 0;
                flag = 1;
            } else {
                flag = 0;
            }
        }
    };


    action get_notification_cnt() {
        ig_md.agent_changed = cmp_ncnt.execute(ig_md.lock_index);
    }

    action new_notification() {
        count_ncnt.execute(ig_md.lock_index);
    }

    action reset_notification_cnt() {
        reset_ncnt.execute(ig_md.lock_index);
    }

    action nop() {}

    table counter_table_2 {
        key = {
            hdr.lock.type: exact;
            hdr.lock.mode: exact;
            hdr.lock.old_mode: exact;
        }

        actions = {
            get_notification_cnt;
            reset_notification_cnt;
            new_notification;
            nop;
        }
        
        const entries = {
            // Type Mode Old-mode
            (ACQUIRE, LOCK_SHARED, 0): new_notification();
            (ACQUIRE, LOCK_SHARED, 1): new_notification();

            (TRANSFER, LOCK_SHARED, LOCK_SHARED): get_notification_cnt();
            (TRANSFER, LOCK_SHARED, LOCK_EXCL): reset_notification_cnt();
            (TRANSFER, LOCK_EXCL, LOCK_SHARED): get_notification_cnt();
            (TRANSFER, LOCK_EXCL, LOCK_EXCL): reset_notification_cnt();

            (FREE, LOCK_SHARED, LOCK_SHARED): get_notification_cnt();
            (FREE, LOCK_SHARED, LOCK_EXCL): reset_notification_cnt();
            (FREE, LOCK_EXCL, LOCK_SHARED): get_notification_cnt();
            (FREE, LOCK_EXCL, LOCK_EXCL): reset_notification_cnt();
        }

        const default_action = nop;
        size = 16;
    }

    apply {
        counter_table_2.apply();
    }
}

control CounterTable_3(
	inout header_t hdr,
	inout metadata_t ig_md) {

    Register<bit<8>, lid_t>(SLICE_SIZE, 0) notification_cnt_3;

    RegisterAction<bit<8>, lid_t, bit<8>>(notification_cnt_3) count_ncnt = {
        void apply(inout bit<8> value) {
            value = value + 1;
        }
    };

    RegisterAction<bit<8>, lid_t, bit<8>>(notification_cnt_3) reset_ncnt = {
        void apply(inout bit<8> value) {
            value = 0;
        }
    };

    RegisterAction<bit<8>, lid_t, bit<1>>(notification_cnt_3) cmp_ncnt = {
        void apply(inout bit<8> value, out bit<1> flag) {
            if (value == hdr.lock.ncnt) {
                value = 0;
                flag = 1;
            } else {
                flag = 0;
            }
        }
    };


    action get_notification_cnt() {
        ig_md.agent_changed = cmp_ncnt.execute(ig_md.lock_index);
    }

    action new_notification() {
        count_ncnt.execute(ig_md.lock_index);
    }

    action reset_notification_cnt() {
        reset_ncnt.execute(ig_md.lock_index);
    }

    action nop() {}

    table counter_table_3 {
        key = {
            hdr.lock.type: exact;
            hdr.lock.mode: exact;
            hdr.lock.old_mode: exact;
        }

        actions = {
            get_notification_cnt;
            reset_notification_cnt;
            new_notification;
            nop;
        }
        
        const entries = {
            // Type Mode Old-mode
            (ACQUIRE, LOCK_SHARED, 0): new_notification();
            (ACQUIRE, LOCK_SHARED, 1): new_notification();

            (TRANSFER, LOCK_SHARED, LOCK_SHARED): get_notification_cnt();
            (TRANSFER, LOCK_SHARED, LOCK_EXCL): reset_notification_cnt();
            (TRANSFER, LOCK_EXCL, LOCK_SHARED): get_notification_cnt();
            (TRANSFER, LOCK_EXCL, LOCK_EXCL): reset_notification_cnt();

            (FREE, LOCK_SHARED, LOCK_SHARED): get_notification_cnt();
            (FREE, LOCK_SHARED, LOCK_EXCL): reset_notification_cnt();
            (FREE, LOCK_EXCL, LOCK_SHARED): get_notification_cnt();
            (FREE, LOCK_EXCL, LOCK_EXCL): reset_notification_cnt();
        }

        const default_action = nop;
        size = 16;
    }

    apply {
        counter_table_3.apply();
    }
}

