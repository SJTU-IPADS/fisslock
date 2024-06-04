
#ifndef __FISSLOCK_CONFIG_H
#define __FISSLOCK_CONFIG_H

#include <types.h>

/////////////////////////////////////////////////////////////////////////
// Adjustable configurations, can be modified to tune program behaviors.
/////////////////////////////////////////////////////////////////////////

/* The number of hosts (machines) in the cluster.
 * Due to the limited switch port number, the number of hosts is
 * at most 32.
 */
#define HOST_NUM             8

/* The number of cores allocated for DPDK TX and RX.
 * We assign 1 core to the lock server daemon and 10 cores to
 * lock clients for sending and receiving lock packets.
 */
#define DPDK_LCORE_NUM           12
#define DPDK_LCORE_RX_SERVER_NUM 1
#define DPDK_LCORE_RX_CLIENT_NUM 5
#define DPDK_LCORE_TX_NUM        5
#define DPDK_LCORE_RX_NUM \
  (DPDK_LCORE_RX_SERVER_NUM + DPDK_LCORE_RX_CLIENT_NUM)

/* Determines the waiting mechanism.
 *
 * - LOCK_WAIT_BUSY_LOOP: when the lock is granted, an atomic flag is set
 *                        without any notification. The client needs to check 
 *                        the flag value in a loop.
 * - LOCK_WAIT_COND_VAR: clients wait on a condition variable when acquiring
 *                       the lock, which is notified when the lock is granted.
 */
#define LOCK_WAIT_BUSY_LOOP 0
#define LOCK_WAIT_COND_VAR  1
#define LOCK_WAIT_MECHANISM LOCK_WAIT_BUSY_LOOP

/* Whether enable precise latency measurement.
 * If not enabled, the latency of coroutine scheduling will also be counted
 * in the end-to-end latency, which incurs bias.
 */
#define LOCK_LATENCY_PRECISE

/* Control reader-writer preferring policy of baselines.
 */
#define PARLOCK_READER_FIRST
#define NETLOCK_READER_FIRST

/* Control lock queue related behaviors of lock manager.
*/
// #define PARLOCK_ENQUEUE_HOLDERS

/* Enable FissLock-Local or not.
 */
// #define TPCC_ENB_LOCAL
#define TPCC_WAREHOUSE_LOCK_NUM     121
#define TPCC_PER_THREAD_CLIENT_NUM  30
#define TPCC_PER_HOST_LOCK_NUM      (TPCC_WAREHOUSE_LOCK_NUM * \
                                     TPCC_PER_THREAD_CLIENT_NUM * \
                                     DPDK_LCORE_TX_NUM)

/* We can make all lock requests out-of-range to naturally
 * fall back to parlock. This should only be enabled during testing.
 */
// #define FALLBACK_TO_PARLOCK

/* Enable counter in statistics.
 */
#define FISSLOCK_COUNTERS

/* Thpt config.
*/
// #define RECORD_THPT_IN_TICK
#define THPT_RECORD_TICK_MS    10
#define THPT_RECORD_DURATION_S 10
#define START_PHASE_POINT_S    7
#define PHASE_DURATION_MS      20
#define PHASE_LOCK_NUM         2500

/* The lock num on switch.
 */
#define SWITCH_LOCK_NUM (524288 * 3)

/* Enable tracing lock queue size of lock manager
*/
#define PARLOCK_TRACE_LOCK_QUEUE_SIZE

/* Turn on/off failure recovery service.
 */
// #define FISSLOCK_FAILURE_RECOVERY
#define FAILURE_RPC_SERVER_ADDR "192.168.12.147:8846"

/////////////////////////////////////////////////////////////////////////
// Constant configurations, do not change in normal cases.
/////////////////////////////////////////////////////////////////////////

/* Environment variables.
 */
#define ENV_TRACE_FILE     "TRACE_FILE"
#define ENV_COROUTINE_NUM  "NUM_CRT"
#define ENV_THINK_TIME     "THINK_TIME"
#define ENV_MAX_LOCK_NUM   "MAX_LOCK_NUM"
#define ENV_HOST_ID        "HOST_ID"

#define FILE_FAULT_SIGNAL  "/tmp/fisslock_fault"

#define LOAD_AND_CHECK_ENV(env) ({\
  char* _env_str = getenv(env);\
  if (!_env_str) EXCEPTION("%s not provided", #env);\
  (_env_str);\
})

/* The ID of ports used to synchronize lock clients.
 */
#define STARTUP_PORT_BASE    9200

/* Flags for configuring the lock module, used when calling
 * lock_setup.
 */
#define FLAG_ENB_LOCAL_GRANT 0x1

/////////////////////////////////////////////////////////////////////////
// Dynamic configurations, can be adjusted via env variables.
/////////////////////////////////////////////////////////////////////////

typedef struct {
  int max_lock_num;
  host_id localhost_id;

  int tx_core_num;
  int rx_core_num;
} configurations;

extern configurations conf;

#define MAX_LOCK_NUM (conf.max_lock_num)
#define LOCALHOST_ID (conf.localhost_id)
#define TX_CORE_NUM  (conf.tx_core_num)
#define RX_CORE_NUM  (conf.rx_core_num)

#ifdef __cplusplus
extern "C" {
#endif

int env_setup(int argc, char* argv[], int tx_num, int rx_num);

#ifdef __cplusplus
}
#endif

#endif
