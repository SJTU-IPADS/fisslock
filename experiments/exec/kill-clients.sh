#!/bin/bash

source ~/set-env.sh

echo $PASSWORD | sudo -S pkill -2 microbench
echo $PASSWORD | sudo -S pkill -2 txnbench
echo $PASSWORD | sudo -S pkill -2 server
echo $PASSWORD | sudo -S pkill -2 redis_txn_
