import logging
import random
import time
import sys
import os
import re

from ptf import config
import ptf.testutils as testutils
from bfruntime_client_base_tests import BfRuntimeTest
import bfrt_grpc.client as gc

from config import *

logger = logging.getLogger('Test')

MAX_SLOTS_NUM = 100000

UDP_DSTPORT = 8888


tot_num_lks = 10000
slots_v_list = []
left_bound_list = []
dev_id = 0

ports = [188]

mirror_ids = []



def make_port(pipe, local_port):
    assert(pipe >= 0 and pipe < 4)
    assert(local_port >= 0 and local_port < 72)
    return (pipe << 7) | local_port

def port_to_pipe(port):
    local_port = port & 0x7F
    assert(local_port < 72)
    pipe = (port >> 7) & 0x3
    assert(port == ((pipe << 7) | local_port))
    return pipe

def port_to_pipe_local_port(port):
    return port & 0x7F

swports = []
swports_by_pipe = {}
for device, port, ifname in config["interfaces"]:
    if port == 0: continue
    if port == 64: continue
    pipe = port_to_pipe(port)
    logger.info(device, port, pipe, ifname)
    logger.info(int(testutils.test_param_get('num_pipes')))
    if pipe not in swports_by_pipe:
        swports_by_pipe[pipe] = []
    if pipe in range(int(testutils.test_param_get('num_pipes'))):
        swports.append(port)
        swports.sort()
        swports_by_pipe[pipe].append(port)
        swports_by_pipe[pipe].sort()

if swports == []:
    for pipe in range(int(testutils.test_param_get('num_pipes'))):
        for port in range(1):
            swports.append( make_port(pipe,port) )
cpu_port = 64
logger.info("Using ports:", swports)
sys.stdout.flush()


class AcquireLockTest(BfRuntimeTest):
    def setUp(self):
        self.client_id = 0
        self.p4_name = "netlock"
        self.target = gc.Target(device_id=0, pipe_id=0xffff)
        BfRuntimeTest.setUp(self, self.client_id, self.p4_name)
        self.bfrt_info = self.interface.bfrt_info_get(self.p4_name)
        # scapy_netlock_bindings() # TODO
        self.init_tables()

    def init_tables(self):
        global tot_num_lks
        ipv4_address_list = [0x0a000204, # pro0_1
                             0x0a000201, # pro1_1
                             0x0a000202, # pro2_1
                             0x0a000203, # pro3_1
                             0x0a000205, # pro0_2
                             0x0a000206, # pro1_2
                             0x0a000207, # pro2_2
                             0x0a000208  # pro3_2
        ]
        # server_ip_address_list = [0x0a000205,
        #                           0x0a000206,
        #                           0x0a000207,
        #                           0x0a000208]
        server_ip_address_list = [0x0a000207]
        port_list = [24, # pro0_1
                     16, # pro1_1
                     8, # pro2_1
                     0, # pro3_1
                     56, # pro0_2
                     48, # pro1_2
                     40, # pro2_2
                     32  # pro3_2
        ]
        sid_list = [1, 2, 3, 4, 5, 6, 7, 8]
        # tgt_tenant = [1,2,3, 4,5,6, 7,8,9, 10,11,0, 1] # TODO
        
        ### TODO
        fix_src_port = []
        for i in range(256):
            fix_src_port.append(9000 + i)
        udp_src_port_list = []
        for i in range(128):
            udp_src_port_list.append(UDP_DSTPORT + i)
        
        ipv4_route_table = self.bfrt_info.table_get("SwitchIngress.ipv4_route_table")
        # ipv4_route_2_table = self.bfrt_info.table_get("SwitchIngress.release_lock.ipv4_route_2")
        ingress_ipv4_route_table_2 = self.bfrt_info.table_get("SwitchIngress.ipv4_route_table_2")
        forward_to_server_table = self.bfrt_info.table_get("SwitchIngress.forward_to_server_table")
        forward_to_server_2_table = self.bfrt_info.table_get("SwitchIngress.acquire_lock.forward_to_server_table")
        forward_to_server_3_table = self.bfrt_info.table_get("SwitchIngress.release_lock.forward_to_server_table")
        # get_tenant_inf_table = self.bfrt_info.table_get("SwitchIngress.check_thres.get_tenant_inf_table")
        acquire_lock_table = self.bfrt_info.table_get("SwitchIngress.acquire_lock.acquire_lock_table")
        dec_empty_slots_table = self.bfrt_info.table_get("SwitchIngress.acquire_lock.dec_empty_slots_table")
        fix_src_port_table = self.bfrt_info.table_get("SwitchIngress.fix_src_port_table")
        fix_src_port_2_table = self.bfrt_info.table_get("SwitchIngress.acquire_lock.fix_src_port_table")
        fix_src_port_3_table = self.bfrt_info.table_get("SwitchIngress.release_lock.fix_src_port_table")
        change_mode_table = self.bfrt_info.table_get("SwitchEgress.change_mode_table")
        set_tag_table = self.bfrt_info.table_get("SwitchIngress.set_tag_table")
        set_tag_2_table = self.bfrt_info.table_get("SwitchIngress.acquire_lock.set_tag_table")
        set_tag_3_table = self.bfrt_info.table_get("SwitchIngress.release_lock.set_tag_table")
        check_lock_exist_table = self.bfrt_info.table_get("SwitchIngress.check_lock_exist_table")
        mirror_cfg_table = self.bfrt_info.table_get("$mirror.cfg")
        release_i2e_mirror_table = self.bfrt_info.table_get("SwitchIngress.release_lock.i2e_mirror_table")
        ingress_i2e_clone_table = self.bfrt_info.table_get("SwitchIngress.i2e_clone_table")
        # acquire_i2e_mirror_table = self.bfrt_info.table_get("SwitchIngress.acquire_lock.i2e_mirror_table")

        ipv4_route_table.info.key_field_annotation_add("hdr.ipv4.dstAddr", "ip")

        # add entries for ipv4 routing
        for i in range(len(ipv4_address_list)):
            ipv4_route_table.entry_add(
                self.target,
                [ipv4_route_table.make_key(
                    [gc.KeyTuple("hdr.ipv4.dstAddr", ipv4_address_list[i])]
                )],
                [ipv4_route_table.make_data(
                    [gc.DataTuple("egress_spec", port_list[i])], 
                    "SwitchIngress.set_egress"
                )]
            )


        # for i in range(len(ipv4_address_list)):
        #     ipv4_route_2_table.entry_add(
        #         self.target,
        #         [ipv4_route_2_table.make_key(
        #             [gc.KeyTuple("hdr.ipv4.dstAddr", ipv4_address_list[i])]
        #         )],
        #         [ipv4_route_2_table.make_data(
        #             [gc.DataTuple("egress_spec", port_list[i])], 
        #             "SwitchIngress.release_lock.set_egress_2"
        #         )]
        #     )

        for i in range(len(ipv4_address_list)):
            ingress_ipv4_route_table_2.entry_add(
                self.target,
                [ingress_ipv4_route_table_2.make_key(
                    [gc.KeyTuple("hdr.ipv4.dstAddr", ipv4_address_list[i])]
                )],
                [ingress_ipv4_route_table_2.make_data(
                    [gc.DataTuple("egress_spec", port_list[i])], 
                    "SwitchIngress.set_egress"
                )]
            )

        for i in range(len(ipv4_address_list)):
            release_i2e_mirror_table.entry_add(
                self.target,
                [release_i2e_mirror_table.make_key(
                    [gc.KeyTuple("hdr.ipv4.dstAddr", ipv4_address_list[i])]
                )],
                [release_i2e_mirror_table.make_data(
                    [gc.DataTuple("mirror_id", sid_list[i])], 
                    "SwitchIngress.release_lock.i2e_mirror_action"
                )]
            )
            ingress_i2e_clone_table.entry_add(
                self.target,
                [ingress_i2e_clone_table.make_key(
                    [gc.KeyTuple("hdr.ipv4.dstAddr", ipv4_address_list[i])]
                )],
                [ingress_i2e_clone_table.make_data(
                    [gc.DataTuple("mirror_id", sid_list[i])], 
                    "SwitchIngress.i2e_clone_action"
                )]
            )
            mirror_cfg_table.entry_add(
                self.target,
                [mirror_cfg_table.make_key(
                    [gc.KeyTuple('$sid', sid_list[i])]
                )],
                [mirror_cfg_table.make_data(
                    [gc.DataTuple('$direction', str_val="INGRESS"),
                    gc.DataTuple('$ucast_egress_port', port_list[i]),
                    gc.DataTuple('$ucast_egress_port_valid', bool_val=True),
                    gc.DataTuple('$session_enable', bool_val=True)],
                    '$normal'
                )]
            )

        # server_node_num = int(testutils.test_param_get('server_node_num'))
        server_node_num = len(server_ip_address_list)

        priority_0 = 1 # TODO
        for i in range(server_node_num):
            forward_to_server_table.entry_add(
                self.target,
                [forward_to_server_table.make_key(
                    [gc.KeyTuple("hdr.nlk_hdr.lock", i, server_node_num - 1), #TODO
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [forward_to_server_table.make_data(
                    [gc.DataTuple("server_ip", server_ip_address_list[i])],
                    "SwitchIngress.forward_to_server_action"
                )]
            ) # TODO
            forward_to_server_2_table.entry_add(
                self.target,
                [forward_to_server_2_table.make_key(
                    [gc.KeyTuple("hdr.nlk_hdr.lock", i, server_node_num - 1),
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [forward_to_server_2_table.make_data(
                    [gc.DataTuple("server_ip", server_ip_address_list[i])],
                    "SwitchIngress.acquire_lock.forward_to_server_action"
                )]
            ) # TODO
            forward_to_server_3_table.entry_add(
                self.target,
                [forward_to_server_3_table.make_key(
                    [gc.KeyTuple("hdr.nlk_hdr.lock", i, server_node_num - 1),
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [forward_to_server_3_table.make_data(
                    [gc.DataTuple("server_ip", server_ip_address_list[i])],
                    "SwitchIngress.release_lock.forward_to_server_action"
                )]
            ) # TODO

        
        # for i in range(len(ipv4_address_list)):
        #     get_tenant_inf_table.entry_add(
        #         self.target,
        #         [get_tenant_inf_table.make_key(
        #             [gc.KeyTuple("hdr.ipv4.srcAddr", ipv4_address_list[i])]
        #         )],
        #         [get_tenant_inf_table.make_data(
        #             [gc.DataTuple("tenant_id", tgt_tenant[i]), 
        #             gc.DataTuple("tenant_threshold", 500000000)], 
        #             "SwitchIngress.check_thres.get_tenant_inf_action"
        #         )]
        #     )

        acquire_lock_table.entry_add(
            self.target,
            [acquire_lock_table.make_key(
                [gc.KeyTuple("hdr.nlk_hdr.mode", SHARED_LOCK)]
            )],
            [acquire_lock_table.make_data(
                [], 
                "SwitchIngress.acquire_lock.acquire_shared_lock_action"
            )]
        )

        acquire_lock_table.entry_add(
            self.target,
            [acquire_lock_table.make_key(
                [gc.KeyTuple("hdr.nlk_hdr.mode", EXCLUSIVE_LOCK)]
            )],
            [acquire_lock_table.make_data(
                [], 
                "SwitchIngress.acquire_lock.acquire_exclusive_lock_action"
            )]
        )

        dec_empty_slots_table.entry_add(
            self.target,
            [dec_empty_slots_table.make_key(
                [gc.KeyTuple("hdr.nlk_hdr.op", 0)]
            )],
            [dec_empty_slots_table.make_data(
                [], 
                "SwitchIngress.acquire_lock.dec_empty_slots_action"
            )]
        ) # normal acquire
        dec_empty_slots_table.entry_add(
            self.target,
            [dec_empty_slots_table.make_key(
                [gc.KeyTuple("hdr.nlk_hdr.op", 2)]
            )],
            [dec_empty_slots_table.make_data(
                [], 
                "SwitchIngress.acquire_lock.push_back_action"
            )]
        ) # server push back

        priority_0 = 1
        for i in range(len(fix_src_port)):
            fix_src_port_table.entry_add(
                self.target,
                [fix_src_port_table.make_key(
                    [gc.KeyTuple("hdr.nlk_hdr.lock", i, len(fix_src_port) - 1), 
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [fix_src_port_table.make_data(
                    [gc.DataTuple("fix_port", fix_src_port[i])], 
                    "SwitchIngress.fix_src_port_action"
                )]
            )
            fix_src_port_2_table.entry_add(
                self.target,
                [fix_src_port_2_table.make_key(
                    [gc.KeyTuple("hdr.nlk_hdr.lock", i, len(fix_src_port) - 1), 
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [fix_src_port_2_table.make_data(
                    [gc.DataTuple("fix_port", fix_src_port[i])], 
                    "SwitchIngress.acquire_lock.fix_src_port_action"
                )]
            )
            fix_src_port_3_table.entry_add(
                self.target,
                [fix_src_port_3_table.make_key(
                    [gc.KeyTuple("hdr.nlk_hdr.lock", i, len(fix_src_port) - 1), 
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [fix_src_port_3_table.make_data(
                    [gc.DataTuple("fix_port", fix_src_port[i])], 
                    "SwitchIngress.release_lock.fix_src_port_action"
                )]
            )

        for i in range(len(udp_src_port_list)):
            change_mode_table.entry_add(
                self.target,
                [change_mode_table.make_key(
                    [gc.KeyTuple("eg_md.tid", i, len(udp_src_port_list) - 1),
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [change_mode_table.make_data(
                    [gc.DataTuple("udp_src_port", udp_src_port_list[i])],
                    "SwitchEgress.change_mode_act"
                )]
            )

        set_tag_table.entry_add(
            self.target,
            [set_tag_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 0),
                gc.KeyTuple("ig_md.lock_exist", 0)]
            )],
            [set_tag_table.make_data(
                [],
                "SwitchIngress.set_as_primary_action"
            )]
        )
        set_tag_table.entry_add(
            self.target,
            [set_tag_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 0),
                gc.KeyTuple("ig_md.lock_exist", 1)]
            )],
            [set_tag_table.make_data(
                [],
                "SwitchIngress.set_as_secondary_action"
            )]
        )
        set_tag_table.entry_add(
            self.target,
            [set_tag_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 1),
                gc.KeyTuple("ig_md.lock_exist", 0)]
            )],
            [set_tag_table.make_data(
                [],
                "SwitchIngress.set_as_primary_action"
            )]
        )
        set_tag_table.entry_add(
            self.target,
            [set_tag_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 1),
                gc.KeyTuple("ig_md.lock_exist", 1)]
            )],
            [set_tag_table.make_data(
                [],
                "SwitchIngress.set_as_failure_notification_action"
            )]
        )
        set_tag_2_table.entry_add(
            self.target,
            [set_tag_2_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 0),
                gc.KeyTuple("ig_md.lock_exist", 0)]
            )],
            [set_tag_2_table.make_data(
                [],
                "SwitchIngress.acquire_lock.set_as_primary_action"
            )]
        )
        set_tag_2_table.entry_add(
            self.target,
            [set_tag_2_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 0),
                gc.KeyTuple("ig_md.lock_exist", 1)]
            )],
            [set_tag_2_table.make_data(
                [],
                "SwitchIngress.acquire_lock.set_as_secondary_action"
            )]
        )
        set_tag_2_table.entry_add(
            self.target,
            [set_tag_2_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 1),
                gc.KeyTuple("ig_md.lock_exist", 0)]
            )],
            [set_tag_2_table.make_data(
                [],
                "SwitchIngress.acquire_lock.set_as_primary_action"
            )]
        )
        set_tag_2_table.entry_add(
            self.target,
            [set_tag_2_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 1),
                gc.KeyTuple("ig_md.lock_exist", 1)]
            )],
            [set_tag_2_table.make_data(
                [],
                "SwitchIngress.acquire_lock.set_as_failure_notification_action"
            )]
        )
        set_tag_3_table.entry_add(
            self.target,
            [set_tag_3_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 0),
                gc.KeyTuple("ig_md.lock_exist", 0)]
            )],
            [set_tag_3_table.make_data(
                [],
                "SwitchIngress.release_lock.set_as_primary_action"
            )]
        )
        set_tag_3_table.entry_add(
            self.target,
            [set_tag_3_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 0),
                gc.KeyTuple("ig_md.lock_exist", 1)]
            )],
            [set_tag_3_table.make_data(
                [],
                "SwitchIngress.release_lock.set_as_secondary_action"
            )]
        )
        set_tag_3_table.entry_add(
            self.target,
            [set_tag_3_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 1),
                gc.KeyTuple("ig_md.lock_exist", 0)]
            )],
            [set_tag_3_table.make_data(
                [],
                "SwitchIngress.release_lock.set_as_primary_action"
            )]
        )
        set_tag_3_table.entry_add(
            self.target,
            [set_tag_3_table.make_key(
                [gc.KeyTuple("ig_md.failure_status", 1),
                gc.KeyTuple("ig_md.lock_exist", 1)]
            )],
            [set_tag_3_table.make_data(
                [],
                "SwitchIngress.release_lock.set_as_failure_notification_action"
            )]
        )

        # write registers
        slots_two_sides_register = self.bfrt_info.table_get("slots_two_sides_register")
        shared_and_exclusive_count_register = self.bfrt_info.table_get("shared_and_exclusive_count_register")
        left_bound_register = self.bfrt_info.table_get("left_bound_register")
        right_bound_register = self.bfrt_info.table_get("right_bound_register")
        head_register = self.bfrt_info.table_get("head_register")
        tail_register = self.bfrt_info.table_get("tail_register")
        queue_size_op_register = self.bfrt_info.table_get("queue_size_op_register")
        

        tot_lk = int(testutils.test_param_get('lk'))
        # tot_lk = 10000
        hmap = [0 for i in range(tot_lk + 1)]
        
        if (testutils.test_param_get('slot') != None):
            slot_num = int(testutils.test_param_get('slot'))
        else:
            slot_num = MAX_SLOTS_NUM

        hash_v = 0
        # task_id = testutils.test_param_get('task_id')
        if (testutils.test_param_get('bm') == 'micro'):
            print("setup micro benchmark")
            # microbenchmark exclusive lock low contention
            tot_num_lks = tot_lk
            qs = 0
            if tot_lk != 0:
                qs = slot_num / tot_lk
            for i in range(1, tot_lk + 1):
                left_bound_register.entry_add(
                    self.target,
                    [left_bound_register.make_key(
                        [gc.KeyTuple("$REGISTER_INDEX", i)]
                    )],
                    [left_bound_register.make_data(
                        [gc.DataTuple("left_bound_register.f1", qs*(i-1) + 1)]
                    )]
                )
                left_bound_list.append(qs*(i-1) + 1)

                right_bound_register.entry_add(
                    self.target,
                    [right_bound_register.make_key(
                        [gc.KeyTuple("$REGISTER_INDEX", i)]
                    )],
                    [right_bound_register.make_data(
                        [gc.DataTuple("right_bound_register.f1", qs*i)]
                    )]
                )

                head_register.entry_add(
                    self.target,
                    [head_register.make_key(
                        [gc.KeyTuple("$REGISTER_INDEX", i)]
                    )],
                    [head_register.make_data(
                        [gc.DataTuple("head_register.f1", qs*(i-1) + 1)]
                    )]
                )

                tail_register.entry_add(
                    self.target,
                    [tail_register.make_key(
                        [gc.KeyTuple("$REGISTER_INDEX", i)]
                    )],
                    [tail_register.make_data(
                        [gc.DataTuple("tail_register.f1", qs*(i-1) + 1)]
                    )]
                )

                shared_and_exclusive_count_register.entry_add(
                    self.target,
                    [shared_and_exclusive_count_register.make_key(
                        [gc.KeyTuple("$REGISTER_INDEX", i)]
                    )],
                    [shared_and_exclusive_count_register.make_data(
                        [gc.DataTuple("shared_and_exclusive_count_register.lo", 0),
                        gc.DataTuple("shared_and_exclusive_count_register.hi", 0)]
                    )]
                )

                queue_size_op_register.entry_add(
                    self.target,
                    [queue_size_op_register.make_key(
                        [gc.KeyTuple("$REGISTER_INDEX", i)]
                    )],
                    [queue_size_op_register.make_data(
                        [gc.DataTuple("queue_size_op_register.f1", 0)]
                    )]
                )

                slots_two_sides_register.entry_add(
                    self.target,
                    [slots_two_sides_register.make_key(
                        [gc.KeyTuple("$REGISTER_INDEX", i)]
                    )],
                    [slots_two_sides_register.make_data(
                        [gc.DataTuple("slots_two_sides_register.lo", qs),
                        gc.DataTuple("slots_two_sides_register.hi", 0)]
                    )]
                )

                # CHANGE according to memory management
                check_lock_exist_table.entry_add(
                    self.target,
                    [check_lock_exist_table.make_key(
                        [gc.KeyTuple("hdr.nlk_hdr.lock", i)]
                    )],
                    [check_lock_exist_table.make_data(
                        [gc.DataTuple("index", i)], 
                        "SwitchIngress.check_lock_exist_action"
                    )]
                )
            head_register.operations_execute(self.target, 'Sync')
            tail_register.operations_execute(self.target, 'Sync')
        else:   
            if (testutils.test_param_get('slot') != None):
                slot_num = int(testutils.test_param_get('slot'))
            else:
                slot_num = MAX_SLOTS_NUM
            main_dir = testutils.test_param_get('main_dir')

            filename = main_dir + "len_in_switch/" + testutils.test_param_get('bm')
            print("Input filename:",filename)
            if (filename != "null"):
                fin = open(filename)
                start_bound = 0
                lines = fin.readlines()
                lock_counter = 0
                for line in lines:
                    re_result = re.search(r"(.*),(.*)", line)
                    lk = int(re_result.group(1))
                    hash_v = int(re_result.group(1))
                    lk_num = int(re_result.group(2))
                    left_bound_register.entry_add(
                        self.target,
                        [left_bound_register.make_key(
                            [gc.KeyTuple("$REGISTER_INDEX", hash_v)]
                        )],
                        [left_bound_register.make_data(
                            [gc.DataTuple("left_bound_register.f1", start_bound + 1)]
                        )]
                    )
                    left_bound_list.append(start_bound + 1)

                    right_bound_register.entry_add(
                        self.target,
                        [right_bound_register.make_key(
                            [gc.KeyTuple("$REGISTER_INDEX", hash_v)]
                        )],
                        [right_bound_register.make_data(
                            [gc.DataTuple("right_bound_register.f1", start_bound + lk_num)]
                        )]
                    )

                    head_register.entry_add(
                        self.target,
                        [head_register.make_key(
                            [gc.KeyTuple("$REGISTER_INDEX", hash_v)]
                        )],
                        [head_register.make_data(
                            [gc.DataTuple("head_register.f1", start_bound + 1)]
                        )]
                    )

                    tail_register.entry_add(
                        self.target,
                        [tail_register.make_key(
                            [gc.KeyTuple("$REGISTER_INDEX", hash_v)]
                        )],
                        [tail_register.make_data(
                            [gc.DataTuple("tail_register.f1", start_bound + 1)]
                        )]
                    )

                    shared_and_exclusive_count_register.entry_add(
                        self.target,
                        [shared_and_exclusive_count_register.make_key(
                            [gc.KeyTuple("$REGISTER_INDEX", hash_v)]
                        )],
                        [shared_and_exclusive_count_register.make_data(
                            [gc.DataTuple("shared_and_exclusive_count_register.lo", 0),
                            gc.DataTuple("shared_and_exclusive_count_register.hi", 0)]
                        )]
                    )

                    queue_size_op_register.entry_add(
                        self.target,
                        [queue_size_op_register.make_key(
                            [gc.KeyTuple("$REGISTER_INDEX", hash_v)]
                        )],
                        [queue_size_op_register.make_data(
                            [gc.DataTuple("queue_size_op_register.f1", 0)]
                        )]
                    )

                    slots_two_sides_register.entry_add(
                        self.target,
                        [slots_two_sides_register.make_key(
                            [gc.KeyTuple("$REGISTER_INDEX", hash_v)]
                        )],
                        [slots_two_sides_register.make_data(
                            [gc.DataTuple("slots_two_sides_register.lo", lk_num),
                            gc.DataTuple("slots_two_sides_register.hi", 0)]
                        )]
                    )

                    check_lock_exist_table.entry_add(
                        self.target,
                        [check_lock_exist_table.make_key(
                            [gc.KeyTuple("hdr.nlk_hdr.lock", lk)]
                        )],
                        [check_lock_exist_table.make_data(
                            [gc.DataTuple("index", hash_v)], 
                            "SwitchIngress.check_lock_exist_action"
                        )]
                    )
                    start_bound = start_bound + lk_num
                    lock_counter += 1
                    if lock_counter >= tot_lk:
                        break
                fin.close()

    def runTest(self):
        pass