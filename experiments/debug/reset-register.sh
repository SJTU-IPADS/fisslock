#!/bin/bash

source ~/set-lcs-env.sh

ssh $USERNAME@$SWITCH "cd /bf-sde-9.5.2;source ./set_sde.bash;echo '$PASSWORD' | sudo -S -E ./run_p4_tests.sh -t /home/chengke/inl_test -p inl --test-params="register_index=$1" --no-veth --no-status-srv"