#!/bin/bash

echo "Project root: ${FISSLOCK_PATH:?'

  The project root is not set. Please run `source experiments/set-env.sh` 
  before executing this script.
'}"

system_name=$1
benchmark=$2
think_time=$3
crt_num=$4

# Identify the test driver and the server binary we are going to use.
if `echo "$benchmark" | grep -E -e "(tpcc)|(tatp)"`; then
  binary_prefix=$FISSLOCK_PATH/build/txnbench
elif `echo "$benchmark" | grep -E -q "redis"`; then
  binary_prefix=$FISSLOCK_PATH/build/redis_txn
else
  binary_prefix=$FISSLOCK_PATH/build/microbench
fi
server_binary=$FISSLOCK_PATH/build/server_netlock

# For NetLock and SrvLock, we need to start the switch control plane
# in advance via python scripts.
netlock_map_obj=0
[ "$system_name" == "netlock" ] && {
  netlock_map_obj=1;
  python3 $MASTER_FISSLOCK_PATH/experiments/netlock/console.py run_netlock_control $benchmark 10000
  ssh $SWITCH "cd $BF_SDE_PATH; source set_sde.bash; ./install/bin/bfshell -f ./init_ports.bfsh"
}

[ "$system_name" == "srvlock" ] && {
  python3 $MASTER_FISSLOCK_PATH/experiments/netlock/console.py run_central_srv_control "micro" 0
  ssh $SWITCH "cd $BF_SDE_PATH; source set_sde.bash; ./install/bin/bfshell -f ./init_ports.bfsh"
}

# Calculate the maximum lock amount in the workload for ParLock to
# partition locks.
max_lock_num=0
if echo $benchmark | grep -q "lkscale"; then
  lks=`echo $benchmark | awk -F'lkscale-' '{print $2}' | awk -F'm' '{print $1}'`
  max_lock_num=`expr $lks \* 1000000`
elif echo $benchmark | grep -q "tpcc"; then
  host_num=8
  wh_num=1200
  lks_per_wh=121
  max_lock_num=`expr $host_num \* $wh_num \* $lks_per_wh`
elif echo $benchmark | grep -q "tatp"; then
  max_lock_num=681000
elif echo $benchmark | grep -q "1000wl"; then
  max_lock_num=10000000
else
  max_lock_num=1000000
fi

echo "Running benchmark: $benchmark"
echo "Max lock num: $max_lock_num"

# The aliases for running clients and the lock server.
run_client="$FISSLOCK_PATH/experiments/exec/run-client.sh $benchmark"
run_server="$FISSLOCK_PATH/experiments/exec/run-server.sh $benchmark"
arguments="$binary_prefix $system_name $crt_num $think_time $max_lock_num $netlock_map_obj"
server_arguments="$server_binary $system_name $max_lock_num $netlock_map_obj"

##############################################################################
# This part of logic is cluster-specific. Please adjust it according 
# to your own cluster setting,
numa_1_cores=0@0,1@2,2@4,3@6,4@8,5@10,6@12,7@14,8@16,9@18,10@20,11@22
numa_2_cores=0@1,1@3,2@5,3@7,4@9,5@11,6@13,7@15,8@17,9@19,10@21,11@23

# ssh <hostname> "$run_client <hid> <nic_id> $numa_x_cores <numa_id> $arguments"
ssh pro0 "$run_client 1 4b:00.0 $numa_1_cores 0 $arguments" &
ssh pro1 "$run_client 2 4b:00.0 $numa_1_cores 0 $arguments" &
ssh pro2 "$run_client 3 17:00.0 $numa_1_cores 0 $arguments" &
ssh pro3 "$run_client 4 17:00.0 $numa_1_cores 0 $arguments" &
ssh pro0 "$run_client 5 b1:00.0 $numa_2_cores 1 $arguments" &
ssh pro1 "$run_client 6 b1:00.0 $numa_2_cores 1 $arguments" &
ssh pro2 "$run_client 7 b1:00.0 $numa_2_cores 1 $arguments" &

if [ "$system_name" == "srvlock" ] || [ "$system_name" == "netlock" ]; then
  ssh pro3 "$run_server 8 b1:00.0 $numa_2_cores 1 $server_arguments" &
else
  ssh pro3 "$run_client 8 b1:00.0 $numa_2_cores 1 $arguments" &
fi

# Wait until all clients have started. The time interval is machine-specific,
# make sure it is long enough for all clients to boot.
sleep 10
###############################################################################

# Send the startup signal to all clients.
python3 $MASTER_FISSLOCK_PATH/experiments/exec/send-signal.py start
