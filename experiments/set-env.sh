
# The username and password on all machines in the cluster,
# including the switch control plane.
export USERNAME='zhz'
export PASSWORD='hanzezhang'

# The paths to the project source code, benchmark traces, and 
# system output. Must be identical on all machines in the cluster.
export FISSLOCK_PATH=/home/zhz/fisslock
export FISSLOCK_TRACE_PATH=$FISSLOCK_PATH/traces
export FISSLOCK_LOG_PATH=$FISSLOCK_PATH/log

# There can be a master machine which controls the cluster
# to run the experiments as well as store the experiment results.
export MASTER_FISSLOCK_PATH=/home/zhz/fisslock
export RESULT_PATH=/home/zhz/fisslock_results

# The hostname and ID of machines in the cluster.
# Each machine is represented by a "hostname-ID" pair.
export HOSTS="pro0-1 pro0-5 pro1-2 pro1-6 pro2-3 pro2-7 pro3-4 pro3-8"
export SWITCH="192.168.12.94"

# The path for BF SDE.
export BF_SDE_PATH="/bf-sde-9.5.2"