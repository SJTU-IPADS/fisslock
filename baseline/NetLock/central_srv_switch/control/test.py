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


tot_num_lks = 0
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
        self.p4_name = "central_srv"
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
        server_ip_address_list = [0x0a000208]
        port_list = [24, # pro0_1
                     16, # pro1_1
                     4, # pro2_1
                     12, # pro3_1
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
        forward_to_server_table = self.bfrt_info.table_get("SwitchIngress.forward_to_server_table")
        fix_src_port_table = self.bfrt_info.table_get("SwitchIngress.fix_src_port_table")

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
                    
        priority_0 = 1
        for i in range(len(fix_src_port)):
            fix_src_port_table.entry_add(
                self.target,
                [fix_src_port_table.make_key(
                    [gc.KeyTuple("ig_md.switch_counter", i, len(fix_src_port) - 1), 
                    gc.KeyTuple("$MATCH_PRIORITY", priority_0)]
                )],
                [fix_src_port_table.make_data(
                    [gc.DataTuple("fix_port", fix_src_port[i])], 
                    "SwitchIngress.fix_src_port_action"
                )]
            )

                
    def runTest(self):
        pass