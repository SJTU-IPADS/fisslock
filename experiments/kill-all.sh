#!/bin/bash

echo "Project root: ${FISSLOCK_PATH:?'

  The project root is not set. Please run `source experiments/set-env.sh` 
  before executing this script.
'}"

targets=`echo $HOSTS | tr ' ' '\n' | awk -F'-' '{print $1}' | uniq`

for host in $targets; do
  ssh $host "$FISSLOCK_PATH/experiments/exec/kill-clients.sh" &
done