#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include <bf_rt/bf_rt.h>
#include <bfutils/clish/thread.h>
#include <bf_switchd/bf_switchd.h>
#include <bfsys/bf_sal/bf_sys_intf.h>
#include <mc_mgr/mc_mgr_intf.h>

#include "bfrt.h"

#define ALL_PIPES 0xffff

/********************************************************* 
 * MAT entry manipulation utilities.                     *
 *********************************************************/

#define prepare_key(table, key_name, key_val, table_key) do {\
  bf_rt_table_key_reset(table.handle, &table_key);\
  bf_status_t bf_status = bf_rt_key_field_set_value(\
      table_key, table.key_id.key_name, key_val);\
  bf_sys_assert(bf_status == BF_SUCCESS);\
} while (0)

#define prepare_arg(table, action_name, arg_name, data_val, table_data) do {\
  bf_rt_table_action_data_reset(\
    table.handle, table.action.action_name.id, &table_data);\
  bf_status_t bf_status = bf_rt_data_field_set_value(\
    table_data, table.action.action_name.args_id.arg_name, data_val);\
  bf_sys_assert(bf_status == BF_SUCCESS);\
} while (0)

#define prepare_reg_data(table, value_name, data_val, table_data) do {\
  bf_rt_table_data_reset(table.handle, &table_data);\
  bf_status_t bf_status = bf_rt_data_field_set_value(\
    table_data, table.value_id.value_name, data_val);\
  bf_sys_assert(bf_status == BF_SUCCESS);\
} while (0)

#define x_tbl_entry(tbl, action, key_name, key_val, arg_name, arg_val, op) do {\
  bf_status_t status = BF_SUCCESS;\
  prepare_key(tbl, key_name, key_val, tbl.tbl_key);\
  prepare_arg(tbl, action, arg_name, arg_val, tbl.tbl_data);\
  status = bf_rt_table_entry_##op(tbl.handle, \
    bfrt_session, &bfrt_target, tbl.tbl_key, tbl.tbl_data);\
  bf_sys_assert(status == BF_SUCCESS);\
  bf_rt_session_complete_operations(bfrt_session);\
} while (0)

#define add_tbl_entry(tbl, action, key_name, key_val, arg_name, arg_val)\
  x_tbl_entry(tbl, action, key_name, key_val, arg_name, arg_val, add)

#define mod_tbl_entry(tbl, action, key_name, key_val, arg_name, arg_val)\
  x_tbl_entry(tbl, action, key_name, key_val, arg_name, arg_val, mod)

#define reg_arr_mod(table, key_t, key, data) do { \
  bf_status_t status = BF_SUCCESS; \
  prepare_key(table, key_t, key, table.tbl_key); \
  prepare_reg_data(table, value, data, table.tbl_data); \
  status = bf_rt_table_entry_mod(table.handle, \
    bfrt_session, &bfrt_target, table.tbl_key, table.tbl_data); \
  bf_sys_assert(status == BF_SUCCESS); \
  bf_rt_session_complete_operations(bfrt_session); \
} while (0)

#define reg_arr_sync(table) do { \
  bf_status_t bf_status = BF_SUCCESS; \
  bf_rt_table_operations_hdl *ope_hdl; \
  bf_status = bf_rt_table_operations_allocate( \
    table.handle, BFRT_REGISTER_SYNC, &ope_hdl); \
  bf_sys_assert(bf_status == BF_SUCCESS); \
  bf_status = bf_rt_operations_register_sync_set( \
    ope_hdl, bfrt_session, &bfrt_target, NULL, NULL); \
  bf_sys_assert(bf_status == BF_SUCCESS); \
  bf_status = bf_rt_table_operations_execute(table.handle, ope_hdl); \
  bf_sys_assert(bf_status == BF_SUCCESS); \
} while (0)

/********************************************************* 
 * Table preparation utilities.                          *
 *********************************************************/

#define table_init(name, table) do {\
  bf_status_t bf_status = bf_rt_table_from_name_get(\
    bfrt_info, P4_TABLE(name), &(table.handle));\
  bf_sys_assert(bf_status == BF_SUCCESS);\
  bf_status = bf_rt_table_key_allocate(table.handle, &table.tbl_key); \
  bf_sys_assert(bf_status == BF_SUCCESS); \
  bf_status = bf_rt_table_data_allocate(table.handle, &table.tbl_data); \
  bf_sys_assert(bf_status == BF_SUCCESS); \
} while (0)

#define action_init(name, table, action_idt) do {\
  bf_status_t bf_status = bf_rt_action_name_to_id((table).handle, \
    P4_ACTION(name), &(table).action.action_idt.id);\
  bf_sys_assert(bf_status == BF_SUCCESS);\
} while (0)

#define table_key_init(name, table, key_idt) do {\
  bf_status_t bf_status = bf_rt_key_field_id_get((table).handle, \
    name, &(table).key_id.key_idt);\
  bf_sys_assert(bf_status == BF_SUCCESS);\
} while (0)

#define action_arg_init(name, table, action_idt, arg_idt) do {\
  bf_status_t bf_status = bf_rt_data_field_id_with_action_get( \
    (table).handle, name, \
    (table).action.action_idt.id, \
    &(table).action.action_idt.args_id.arg_idt \
  ); \
  bf_sys_assert(bf_status == BF_SUCCESS); \
} while (0)

#define reg_data_init(name, table) do {\
  bf_status_t bf_status = bf_rt_data_field_id_get((table).handle, \
    P4_REGISTER_DATA(name), &(table).value_id.value); \
  bf_sys_assert(bf_status == BF_SUCCESS); \
} while (0)

/********************************************************* 
 * Table declaration utilities.                          *
 *********************************************************/

#define mat_declare_start(table_name, key_idt) \
  struct { \
    const bf_rt_table_hdl* handle; \
    bf_rt_table_key_hdl* tbl_key; \
    bf_rt_table_data_hdl* tbl_data; \
    \
    struct { \
      bf_rt_id_t key_idt; \
    } key_id; \
    \
    struct {

#define action_declare(action_name) \
  struct { \
    bf_rt_id_t id; \
  } action_name;

#define action_declare_with_arg(action_name, arg_idt) \
  struct { \
    bf_rt_id_t id; \
    struct { \
      bf_rt_id_t arg_idt; \
    } args_id; \
  } action_name;

#define mat_declare_end(table_name, key_idt) \
    } action; \
  } table_name;

#define lock_arr_declare(table_name, key_idt) \
  struct { \
    const bf_rt_table_hdl* handle; \
    bf_rt_table_key_hdl* tbl_key; \
    bf_rt_table_data_hdl* tbl_data; \
    \
    struct { \
      bf_rt_id_t key_idt; \
    } key_id; \
    \
    struct { \
      bf_rt_id_t value; \
    } value_id; \
  } table_name;

/********************************************************* 
 * Barefoot runtime related global data.                 *
 *********************************************************/

const bf_rt_info_hdl* bfrt_info = NULL;
bf_rt_session_hdl* bfrt_session = NULL;
bf_rt_target_t bfrt_target;

/********************************************************* 
 * MAT metadata.                                         *
 *********************************************************/

// Ethernet fallback MAT.
mat_declare_start(mat_eth_fallback, dst_mac)
action_declare_with_arg(eth_forward, port)
mat_declare_end(mat_eth_fallback, dst_mac)

// Forward by host ID MAT.
mat_declare_start(mat_fwd_host, host_id)
action_declare_with_arg(forward_to_host, port)
mat_declare_end(mat_fwd_host, host_id)

/********************************************************* 
 * Lock array metadata.                                  *
 *********************************************************/

lock_arr_declare(reg_arr_lock_states, lid)
lock_arr_declare(reg_arr_lock_acq_states, lid)
lock_arr_declare(reg_arr_lock_owners, lid)

/********************************************************* 
 * Environment setup functions.                          *
 *********************************************************/

/* Set up the barefoot runtime environment.
 * Must be called before other setup functions.
 */
static void setup_bfrt(void) {
  bf_status_t bf_status;

  // We only have one switch ASIC, so the dev_id is 0.
  bfrt_target.dev_id = 0;
  bfrt_target.pipe_id = ALL_PIPES;

  // Locate the p4 program in the ASIC device.
  bf_status = bf_rt_info_get(bfrt_target.dev_id, P4_PROGRAM_NAME, &bfrt_info);
  bf_sys_assert(bf_status == BF_SUCCESS);

  // Create the bfrt session.
  bf_status = bf_rt_session_create(&bfrt_session);
  bf_sys_assert(bf_status == BF_SUCCESS);
}

/* Set up all MATs.
 */
static void setup_tables(void) {

  // Get table objects.
  table_init("eth_fallback", mat_eth_fallback);
  // table_init("forward_by_host_id", mat_fwd_host);

  // table_init("lock_state_array", reg_arr_lock_states);
  // table_init("lock_acq_state_array", reg_arr_lock_acq_states);
  // table_init("lock_agent_array", reg_arr_lock_owners);

  // Get action IDs.
  action_init("eth_forward", mat_eth_fallback, eth_forward);
  // action_init("forward_to_host", mat_fwd_host, forward_to_host);

  // Get table key field ID.
  table_key_init("hdr.ethernet.dst_mac", mat_eth_fallback, dst_mac);
  // table_key_init("hdr.lock.agent", mat_fwd_host, host_id);

  // table_key_init("$REGISTER_INDEX", reg_arr_lock_states, lid);
  // table_key_init("$REGISTER_INDEX", reg_arr_lock_acq_states, lid);
  // table_key_init("$REGISTER_INDEX", reg_arr_lock_owners, lid);

  // Get action argument field ID.
  action_arg_init("port", mat_eth_fallback, eth_forward, port);
  // action_arg_init("port", mat_fwd_host, forward_to_host, port);

  // Get table data field ID.
  // reg_data_init("lock_state_array", reg_arr_lock_states);
  // reg_data_init("lock_acq_state_array", reg_arr_lock_acq_states);
  // reg_data_init("lock_owner_array", reg_arr_lock_owners);

}

/**
 * A helper for creating a multicast group.
 */
void create_mgrp(bf_mc_grp_id_t gid, bf_mc_port_map_t* port_map, 
                 bf_mc_rid_t rid, bf_mc_session_hdl_t shdl) {
  bf_status_t bf_status;
  bf_dev_id_t dev = 0;
  bf_mc_node_hdl_t node_hdl;
  bf_mc_mgrp_hdl_t ghdl;

  // Link Aggregation Group, not used.
  bf_mc_lag_map_t lag_map;
  BF_MC_LAG_MAP_INIT(lag_map);

  bf_status = bf_mc_mgrp_create(shdl, dev, gid, &ghdl);
  bf_sys_assert(bf_status == BF_SUCCESS);

  // rid: see https://github.com/p4lang/tutorials/issues/22.
  bf_status = bf_mc_node_create(
    shdl, dev, rid, *port_map, lag_map, &node_hdl);
  bf_sys_assert(bf_status == BF_SUCCESS);

  bf_status = bf_mc_associate_node(shdl, dev, ghdl, node_hdl, 0, 0);
  bf_sys_assert(bf_status == BF_SUCCESS);
}

/* Set up multicast groups.
 * 
 * The data plane can specify up to two multicast groups, and we only
 * need to multicast to two hosts. Hence, each mgrp we create only
 * contain one host.
 * 
 * To send grant reply and agent notification with different proto_type,
 * we create two mgrps for each host with different rid, so the egress
 * pipeline can handle them differently.
 * 
 */
static void setup_mgrps(void) {
  bf_mc_session_hdl_t shdl;
  bf_status_t bf_status;

  bf_status = bf_mc_init();
  bf_sys_assert(bf_status == BF_SUCCESS);

  bf_status = bf_mc_create_session(&shdl);
  bf_sys_assert(bf_status == BF_SUCCESS);

  // mgrp 1-127: agent notification
  // mgrp 129-191: grant reply
  // mgrp 193-254: broadcast except requests
  // mgrp 255: broadcast
  // 
  // mgrp 0, 128, 192 are reserved to drop replicated packets when
  // we only want unicast.
  for (size_t i = 0; i < MACHINE_NUM; i++) {

    // mgrp 0-191
    // op 1: notification, op 2: reply
    for (int op = 1; op < 3; op++) {
      bf_mc_port_map_t port_map;
      BF_MC_PORT_MAP_INIT(port_map);
      BF_MC_PORT_MAP_SET(port_map, connected_machines[i].port);
      create_mgrp(MID(i) + 128 * (op - 1), &port_map, op, shdl);
    }

    // mgrp 192-254
    bf_mc_port_map_t port_map;
    BF_MC_PORT_MAP_INIT(port_map);
    for (size_t m = 0; m < MACHINE_NUM; m++)
      if (m != i) BF_MC_PORT_MAP_SET(port_map, connected_machines[m].port);
    create_mgrp(MID(i) + 192, &port_map, 0, shdl);
  }

  // mgrp 255
  bf_mc_port_map_t port_map;
  BF_MC_PORT_MAP_INIT(port_map);
  for (size_t m = 0; m < MACHINE_NUM; m++)
    BF_MC_PORT_MAP_SET(port_map, connected_machines[m].port);
  create_mgrp(255, &port_map, 0, shdl);
}


/********************************************************* 
 * Exposed functions.                                    *
 *********************************************************/

/* Initialize the bfrt driver.
 */
void driver_init() {
  setup_bfrt();
  setup_tables();

  // Install ethernet forwarding rules.
  for (size_t i = 0; i < MACHINE_NUM; i++) {
    add_tbl_entry(mat_eth_fallback, eth_forward, 
      dst_mac, connected_machines[i].mac_addr, 
      port, connected_machines[i].port
    );
  }

  // Install host ID to egress port forwarding rules.
  for (size_t i = 0; i < MACHINE_NUM; i++) {
    // printf("Mapping host ID %d to port %d\n", 
    //   machine_host_id[i], connected_machines[i].port);
    // add_tbl_entry(mat_fwd_host, forward_to_host, 
    //   host_id, machine_host_id[i], 
    //   port, connected_machines[i].port
    // );
  }

  setup_mgrps();
}
