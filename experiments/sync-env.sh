#!/bin/bash

echo "Project root: ${FISSLOCK_PATH:?'

  The project root is not set. Please run `source experiments/set-env.sh` 
  before executing this script.
'}"

targets=`echo $HOSTS | tr ' ' '\n' | awk -F'-' '{print $1}' | uniq`

for host in $targets; do
  rsync -a $MASTER_FISSLOCK_PATH/experiments/set-env.sh $host:~/
  ssh $host "mkdir -p $FISSLOCK_PATH $FISSLOCK_TRACE_PATH $FISSLOCK_LOG_PATH"

  echo "syncing to $host..."
  rsync -a $MASTER_FISSLOCK_PATH/build $host:$FISSLOCK_PATH/
  rsync -a $MASTER_FISSLOCK_PATH/experiments $host:$FISSLOCK_PATH/
  rsync -a $MASTER_FISSLOCK_PATH/traces $host:$FISSLOCK_TRACE_PATH/..
done

rsync -a $MASTER_FISSLOCK_PATH/build/netlock_len_in_switch/ \
  $SWITCH:/home/ck/workspace/netlock_reproduce/len_in_switch/