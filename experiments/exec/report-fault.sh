#!/bin/bash

source ~/set-env.sh

code=$1

for host in pro0 pro1 pro2 pro3; do
	ssh $host "echo $PASSWORD | sudo -S $FISSLOCK_PATH/build/report_fault $code"
	#ssh $host "echo $PASSWORD | sudo -S rm /tmp/fisslock_fault"
done
