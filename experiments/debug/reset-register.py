import logging
import random
import time
import sys
import os

from ptf import config
import ptf.testutils as testutils
from bfruntime_client_base_tests import BfRuntimeTest
import bfrt_grpc.client as gc

# from config import *

logger = logging.getLogger('Test')



dev_id = 0
if testutils.test_param_get("arch") == "Tofino":
  logger.info("TYPE Tofino")
  sys.stdout.flush()
  MIR_SESS_COUNT = 1024
  MAX_SID_NORM = 1015
  MAX_SID_COAL = 1023
  BASE_SID_NORM = 1
  BASE_SID_COAL = 1016
elif testutils.test_param_get("arch") == "Tofino2":
  logger.info("TYPE Tofino2")
  sys.stdout.flush()
  MIR_SESS_COUNT = 256
  MAX_SID_NORM = 255
  MAX_SID_COAL = 255
  BASE_SID_NORM = 0
  BASE_SID_COAL = 0
else:
  logger.info("TYPE NONE")
  logger.info(testutils.test_param_get("arch"))
  sys.stdout.flush()

SLICE_SIZE_POW2 = 19
SLICE_SIZE = (1 << SLICE_SIZE_POW2)
SLICE_NUM = 8

reg_index = int(testutils.test_param_get('register_index'))


class RegisterTest(BfRuntimeTest):
    def setUp(self):
        self.client_id = 0
        self.p4_name = "fisslock_decider"
        self.target = gc.Target(device_id=0, pipe_id=0xffff)
        BfRuntimeTest.setUp(self, self.client_id, self.p4_name)
        self.bfrt_info = self.interface.bfrt_info_get(self.p4_name)

    def runTest(self):
        lock_state_array = self.bfrt_info.table_get("IngressPipe.lock_state_array")
        lock_acq_state_array = self.bfrt_info.table_get("IngressPipe.lock_acq_state_array")
        lock_agent_array_1 = self.bfrt_info.table_get("IngressPipe.LockOperation_1.lock_agent_array_1")
        lock_agent_array_2 = self.bfrt_info.table_get("IngressPipe.LockOperation_2.lock_agent_array_2")
        lock_agent_array_3 = self.bfrt_info.table_get("IngressPipe.LockOperation_3.lock_agent_array_3")
        notification_cnt_1 = self.bfrt_info.table_get("IngressPipe.CounterTable_1.notification_cnt_1")
        notification_cnt_2 = self.bfrt_info.table_get("IngressPipe.CounterTable_2.notification_cnt_2")
        notification_cnt_3 = self.bfrt_info.table_get("IngressPipe.CounterTable_3.notification_cnt_3")
        
        resp = lock_state_array.entry_get(
            self.target,
            [lock_state_array.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("lock_state_array", reg_index, data_dict))
                
        resp = lock_acq_state_array.entry_get(
            self.target,
            [lock_acq_state_array.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("lock_acq_state_array", reg_index, data_dict))
        
        resp = lock_agent_array_1.entry_get(
            self.target,
            [lock_agent_array_1.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("lock_agent_array_1", reg_index, data_dict))
            
        resp = lock_agent_array_2.entry_get(
            self.target,
            [lock_agent_array_2.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("lock_agent_array_2", reg_index, data_dict))
        
        resp = lock_agent_array_3.entry_get(
            self.target,
            [lock_agent_array_3.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("lock_agent_array_3", reg_index, data_dict))
            
        resp = notification_cnt_1.entry_get(
            self.target,
            [notification_cnt_1.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("notification_cnt_1", reg_index, data_dict))
        
        resp = notification_cnt_2.entry_get(
            self.target,
            [notification_cnt_2.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("notification_cnt_2", reg_index, data_dict))
        
        resp = notification_cnt_3.entry_get(
            self.target,
            [notification_cnt_3.make_key(
                [gc.KeyTuple("$REGISTER_INDEX", reg_index)]
            )],
            {"from_hw": True}
        )
        for data, key in resp:
            data_dict = data.to_dict()
            print("{}[{}] : {}".format("notification_cnt_3", reg_index, data_dict))