
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>

#include "conf.h"
#include "net.h"
#include "lock.h"
#include "debug.h"

#ifdef FISSLOCK_FAILURE_RECOVERY
#include "rpc.h"
#include "fault.h"
#endif

configurations conf;

int env_setup(int argc, char* argv[], int tx_num, int rx_num) {
  conf.tx_core_num = tx_num;
  conf.rx_core_num = rx_num;

  conf.max_lock_num = atoi(LOAD_AND_CHECK_ENV(ENV_MAX_LOCK_NUM));
  conf.localhost_id = atoi(LOAD_AND_CHECK_ENV(ENV_HOST_ID));

  dpdk_setup(argc, argv);
#ifdef FISSLOCK_FAILURE_RECOVERY
  rpc_setup_cli(FAILURE_RPC_SERVER_ADDR);
  fault_detector_setup();
#endif
  return 0;
}