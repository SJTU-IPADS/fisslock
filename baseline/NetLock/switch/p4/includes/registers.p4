Register<pair, bit<32>>(NUM_LOCKS) slots_two_sides_register; // hi is length, lo is number of empty slots
Register<bit<32>, bit<32>>(LENGTH_ARRAY) timestamp_hi_array_register;
Register<pair, bit<32>>(NUM_LOCKS) shared_and_exclusive_count_register; // hi is shared lock, lo is exclusive lock
Register<bit<32>, bit<32>>(NUM_LOCKS) tail_register;
Register<bit<32>, bit<32>>(LENGTH_ARRAY) ip_array_register;
Register<bit<8>, bit<32>>(LENGTH_ARRAY) mode_array_register;
Register<bit<8>, bit<32>>(LENGTH_ARRAY) client_id_array_register;
Register<bit<32>, bit<32>>(LENGTH_ARRAY) tid_array_register;
Register<bit<32>, bit<32>>(LENGTH_ARRAY) timestamp_lo_array_register;
Register<bit<32>, bit<4>>(NUM_TENANTS, 0) tenant_acq_counter_register;
Register<bit<32>, bit<32>>(NUM_LOCKS) queue_size_op_register;
Register<bit<32>, bit<32>>(NUM_LOCKS) left_bound_register;
Register<bit<32>, bit<32>>(NUM_LOCKS) right_bound_register;
Register<bit<32>, bit<32>>(NUM_LOCKS) head_register;
Register<bit<8>, bit<1>>(1, 0) failure_status_register;
