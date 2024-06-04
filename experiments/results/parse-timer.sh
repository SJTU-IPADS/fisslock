#!/bin/bash

echo "Project root: ${MASTER_FISSLOCK_PATH:?'

  The project root is not set. Please run `source experiments/set-env.sh` 
  before executing this script.
'}"

system=${1:?"Usage: parse-timer.sh [system] [benchmark]"}
benchmark=${2:?"Usage: parse-timer.sh [system] [benchmark]"}

make -C $MASTER_FISSLOCK_PATH/experiments/results/timer_parser

TEST_NAME=$RESULT_PATH/$benchmark/lat-$system \
SYSTEM_NAME=$system \
$MASTER_FISSLOCK_PATH/experiments/results/timer_parser/main