#!/bin/bash

echo "Project root: ${FISSLOCK_PATH:?'

  The project root is not set. Please run `source experiments/set-env.sh` 
  before executing this script.
'}"

system_name=$1
benchmark=$2
think_time=$3
crt_num=$4

$MASTER_FISSLOCK_PATH/experiments/run-on-all.sh $system_name $benchmark $think_time $crt_num 2>tmp

while :
do
    done_cnt=`cat tmp | grep "done" | wc -l`
    if [[ $done_cnt == 40 ]]
    then
        break
    fi
    sleep 1
done

$MASTER_FISSLOCK_PATH/experiments/kill-all.sh

while :
do
    exit_cnt=`cat tmp | grep "exit" | wc -l`
    if [[ $exit_cnt == 8 ]]
    then
        break
    fi
    sleep 1
done

$MASTER_FISSLOCK_PATH/experiments/results/throughput-calculator.sh "throughput"\
    > $RESULT_PATH/$benchmark/thpt

$MASTER_FISSLOCK_PATH/experiments/results/collect-results.sh $system_name $benchmark $think_time