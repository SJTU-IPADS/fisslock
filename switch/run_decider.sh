#!/bin/bash
# Start running switchd application program

function add_hugepage() {
    sudo sh -c 'echo "#Enable huge pages support for DMA purposes" >> /etc/sysctl.conf'
    sudo sh -c 'echo "vm.nr_hugepages = 128" >> /etc/sysctl.conf'
}

function dma_setup() {
    echo "Setting up DMA Memory Pool"
    hp=$(sudo sysctl -n vm.nr_hugepages)

    if [ $hp -lt 128 ]; then
        if [ $hp -eq 0 ]; then
            add_hugepage
        else
            nl=$(egrep -c vm.nr_hugepages /etc/sysctl.conf)
            if [ $nl -eq 0 ]; then
                add_hugepage
            else
                sudo sed -i 's/vm.nr_hugepages.*/vm.nr_hugepages = 128/' /etc/sysctl.conf
            fi
        fi
        sudo sysctl -p /etc/sysctl.conf
    fi

    if [ ! -d /mnt/huge ]; then
        sudo mkdir /mnt/huge
    fi
    sudo mount -t hugetlbfs nodev /mnt/huge
}

OLD_STTY_SETTINGS=`stty -g`
function finish {
   stty $OLD_STTY_SETTINGS
   exit
}
trap finish EXIT

[ -z ${SDE} ] && echo "Environment variable SDE not set" && exit 1
[ -z ${SDE_INSTALL} ] && echo "Environment variable SDE_INSTALL not set" && exit 1

export SDE=${SDE}
export SDE_INSTALL=${SDE_INSTALL}
echo "Using SDE ${SDE}"
echo "Using SDE_INSTALL ${SDE_INSTALL}"

opts=$(getopt -o c:Cp:ghr:s: --long no-status-srv,server-listen-local-only,bfs-local-only,reg-channel-server-local-only,dma-channel-server-local-only,fpga-channel-server-local-only,bfrt-grpc-server-local-only,status-server-local-only,gdb-server,skip-p4,skip-port-add,kernel-pkt,skip-hld:,status-port:,init-mode:,arch: -- "$@")
if [ $? != 0 ]; then
  exit 1
fi
eval set -- "$opts"

# Parse options.
P4_NAME=""
SETUP_SCRIPT=""
DBG=""
CHIP_ARCH="tofino"
STS_PORT_STR="--status-port 7777"
KERNEL_PKT_STR=""
#KERNEL_PKT_STR="--kernel-pkt"

while true; do
    case "$1" in
      -g) DBG="gdb -ex run --args"; shift 1;;
      -p) P4_NAME=$2; shift 2;;
      -s) SETUP_SCRIPT=$2; shift 2;;
      --) shift; break;;
      *) break;;
    esac
done

# Setup DMA.
dma_setup

# Search for configuration file.
TARGET_CONFIG_FILE=$SDE_INSTALL/share/p4/targets/$CHIP_ARCH/$P4_NAME.conf
[ ! -r $TARGET_CONFIG_FILE ] && echo "File $TARGET_CONFIG_FILE not found" && exit 1
echo "Using TARGET_CONFIG_FILE ${TARGET_CONFIG_FILE}"

# Set binary path and library path.
export PATH=$SDE_INSTALL/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/lib:$SDE_INSTALL/lib:$LD_LIBRARY_PATH
echo "Using PATH ${PATH}"
echo "Using LD_LIBRARY_PATH ${LD_LIBRARY_PATH}"

# Start the control plane program.
sudo env "SDE=$SDE" "SDE_INSTALL=$SDE_INSTALL" \
         "PATH=$PATH" "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" \
         $DBG $P4_NAME \
         --install-dir $SDE_INSTALL \
         --conf-file $TARGET_CONFIG_FILE \
         --setup-script $SETUP_SCRIPT
