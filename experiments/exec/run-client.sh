#!/bin/bash

source ~/set-env.sh

benchmark=$1
host_id=$2
device=$3
lcore_map=$4
socket_id=$5
binary=$6
system=$7
num_crt=$8
think_time=$9
max_lock_num=${10}
netlock_map_obj=${11}

echo $PASSWORD | sudo -S -E \
  LD_LIBRARY_PATH=$HOME/.local/lib:$HOME/.local/lib/x86_64-linux-gnu \
  TRACE_FILE=$FISSLOCK_TRACE_PATH/h$host_id-$benchmark.csv \
  NUM_CRT=$num_crt \
  MAP_OBJ=$netlock_map_obj \
  MAP_FILE=$FISSLOCK_PATH/build/netlock_map/$benchmark \
  HOST_ID=$host_id \
  THINK_TIME=$think_time \
  MAX_LOCK_NUM=$max_lock_num \
  numactl --cpubind=$socket_id --membind=$socket_id \
  ${binary}_$system \
  -a $device \
  -n 4 \
  --file-prefix=node$host_id \
  --lcores $lcore_map \
  --log-level=4 \
  >$FISSLOCK_LOG_PATH/$benchmark-$system-h$host_id