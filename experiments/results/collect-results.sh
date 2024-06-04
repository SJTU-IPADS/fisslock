#!/bin/bash

echo "Project root: ${FISSLOCK_PATH:?'

  The project root is not set. Please run `source experiments/set-env.sh` 
  before executing this script.
'}"

system=${1:?"Usage: collect-results.sh [system] [benchmark]"}
benchmark=${2:?"Usage: collect-results.sh [system] [benchmark]"}

input_path=$RESULT_PATH/$benchmark
mkdir -p $input_path

for host in $HOSTS; do
  server=`echo $host | awk -F'-' '{print $1}'`
  hostid=`echo $host | awk -F'-' '{print $2}'`
  scp $USERNAME@$server:$FISSLOCK_LOG_PATH/$benchmark-$system-h$hostid \
      $input_path/$system-h$hostid
done

out_f=$input_path/lat-$system
rm -f $out_f-raw
for host in $HOSTS; do
  hostid=`echo $host | awk -F'-' '{print $2}'`
  cat $input_path/$system-h$hostid >>$out_f-raw
  rm $input_path/$system-h$hostid
done
