#!/bin/bash

source ~/set-env.sh

benchmark=$1
host_id=$2
device=$3
lcore_map=$4
socket_id=$5
binary=$6
system=$7
max_lock_num=$8
netlock_map_obj=$9

echo $PASSWORD | sudo -S -E \
  LD_LIBRARY_PATH=$HOME/.local/lib:$HOME/.local/lib/x86_64-linux-gnu \
  MAP_OBJ=$netlock_map_obj \
  LEN_IN_SWITCH_FILE=$FISSLOCK_PATH/build/netlock_len_in_switch/$benchmark \
  HOST_ID=$host_id \
  MAX_LOCK_NUM=$max_lock_num \
  numactl --cpubind=$socket_id --membind=$socket_id \
  ${binary} \
  -a $device \
  -n 4 \
  --file-prefix=node$host_id \
  --lcores $lcore_map \
  --log-level=4 \
  >$FISSLOCK_LOG_PATH/$benchmark-$system-h$host_id