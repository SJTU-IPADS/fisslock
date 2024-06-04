#!/bin/bash
# Start running switchd application program

function print_help() {
  echo "USAGE: $(basename ""$0"") {-p <...> | -c <...>} [OPTIONS -- SWITCHD_OPTIONS]"
  echo "Options for running switchd:"
  echo "  -p <p4_program_name>"
  echo "    Load driver with artifacts associated with P4 program"
  echo "  -c TARGET_CONFIG_FILE"
  echo "    TARGET_CONFIG_FILE that describes P4 artifacts of the device"
  echo "  -r REDIRECTLOG"
  echo "    logfile to redirect"
  echo "  -C"
  echo "    Start CLI immediately"
  echo "  --skip-p4"
  echo "    Skip loading of P4 program in device"
  echo "  --skip-hld <skip_hld_mgr_list>"
  echo "    Skip high level drivers:"
  echo "    p:pipe_mgr, m:mc_mgr, k:pkt_mgr, r:port_mgr, t:traffic_mgr"
  echo "  --skip-port-add"
  echo "    Skip adding ports"
  echo "  --kernel-pkt"
  echo "    use kernel space packet processing"
  echo "  -h"
  echo "    Print this message"
  echo "  -g"
  echo "    Run with gdb"
  echo "  --gdb-server"
  echo "    Run with gdbserver; Listening on port 12345 "
  echo "  --no-status-srv"
  echo "    Do not start bf_switchd's status server"
  echo "  --status-port <port number>"
  echo "    Specify the port that bf_switchd's status server will use; the default is 7777"
  echo "  -s"
  echo "    Don't stop on the first error when running under the address sanitizer"
  echo "  --init-mode <cold|fastreconfig>"
  echo "    Specifiy if devices should be cold booted or under go fast reconfig"
  echo "  --arch <Tofino|Tofino2>"
  echo "    Specifiy the chip architecture, defaults to Tofino"
  echo "  --server-listen-local-only"
  echo "    Servers can only be connected from localhost"
  exit 0
}

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

opts=$(getopt -o c:Cp:ghr:s --long no-status-srv,server-listen-local-only,bfs-local-only,reg-channel-server-local-only,dma-channel-server-local-only,fpga-channel-server-local-only,bfrt-grpc-server-local-only,status-server-local-only,gdb-server,skip-p4,skip-port-add,kernel-pkt,skip-hld:,status-port:,init-mode:,arch: -- "$@")
if [ $? != 0 ]; then
  exit 1
fi
eval set -- "$opts"

# P4 program name
P4_NAME=""
# debug options
DBG=""
# target config file
TARGET_CONFIG_FILE=""

HELP=false
SKIP_P4=false
SKIP_HLD=""
SKIP_PORT_ADD=false
KERNEL_PKT=false
SKIP_STATUS_SRV=false
ASAN_ON_ERROR=""
INIT_MODE="cold"
REDIRECTLOG=""
CHIP_ARCH="Tofino"
SHELL_NO_WAIT=""
while true; do
    case "$1" in
      -h) HELP=true; shift 1;;
      -g) DBG="gdb -ex run --args"; shift 1;;
      -p) P4_NAME=$2; shift 2;;
      -c) TARGET_CONFIG_FILE=$2; shift 2;;
      -C) SHELL_NO_WAIT="--shell-no-wait"; shift 1;;
      -r) REDIRECTLOG=$2; shift 2;;
      -s) ASAN_ON_ERROR="ASAN_OPTIONS=halt_on_error=0"; shift 1;;
      --gdb-server) DBG="gdbserver :12345 "; shift 1;;
      --arch) CHIP_ARCH=$2; shift 2;;
      --skip-p4) SKIP_P4=true; shift 1;;
      --skip-hld) SKIP_HLD=$2; shift 2;;
      --skip-port-add) SKIP_PORT_ADD=true; shift 1;;
      --kernel-pkt) KERNEL_PKT=true; shift 1;;
      --status-port) STS_PORT=$2; shift 2;;
      --no-status-srv) SKIP_STATUS_SRV=true; shift 1;;
      --init-mode) INIT_MODE=$2; shift 2;;
      --server-listen-local-only) SERVER_LISTEN_LOCAL_ONLY="$1" ;shift 1;;
      # --bfs-local-only) BF_SHELL_SERVER_LOCAL_ONLY="$1"; shift 1;;
      # --status-server-local-only) STATUS_SERVER_LOCAL_ONLY="$1" ;shift 1;;
      # --dma-channel-server-local-only) DMA_CHANNEL_SERVER_LOCAL_ONLY="$1"  ;shift 1;;
      # --reg-channel-server-local-only) REG_CHANNEL_SERVER_LOCAL_ONLY="$1"  ;shift 1;;
      # --fpga-channel-server-local-only) FPGA_CHANNEL_SERVER_LOCAL_ONLY="$1"  ;shift 1;;
      # --bfrt-grpc-server-local-only) BFRT_GRPC_SERVER_LOCAL_ONLY="$1"  ;shift 1;;
      --) shift; break;;
    esac
done

if [ $HELP = true ] || ( [ -z $P4_NAME ] && [ -z $TARGET_CONFIG_FILE ] ); then
  print_help
fi

CHIP_ARCH=`echo $CHIP_ARCH | tr '[:upper:]' '[:lower:]'`
case "$CHIP_ARCH" in
  tofino2) ;;
  tofino) ;;
  *) echo "Invalid arch option specified ${CHIP_ARCH}"; exit 1;;
esac

dma_setup

SKIP_P4_STR=""
if [ $SKIP_P4 = true ]; then
  SKIP_P4_STR="--skip-p4"
fi
SKIP_HLD_STR=""
if [ "$SKIP_HLD" != "" ]; then
  SKIP_HLD_STR="--skip-hld $SKIP_HLD"
fi
SKIP_PORT_ADD_STR=""
if [ $SKIP_PORT_ADD = true ]; then
  SKIP_PORT_ADD_STR="--skip-port-add"
fi

STS_PORT_STR="--status-port 7777"
if [ "$STS_PORT" != "" ]; then
  STS_PORT_STR="--status-port $STS_PORT"
fi
KERNEL_PKT_STR=""
if [ $KERNEL_PKT = true ]; then
  KERNEL_PKT_STR="--kernel-pkt"
fi

if [ $SKIP_STATUS_SRV = true ]; then
  STS_PORT_STR=""
fi

CUSTOM_CONF_FILE="$SDE/$(find -type f -path "*p4-examples*/ptf-tests" | head -n1)/$P4_NAME/custom_conf_file"
if [ ! -f $CUSTOM_CONF_FILE ]; then
    CUSTOM_CONF_FILE="$SDE/$(find -type d -path "*p4-examples*/p4_16_programs" | head -n1)/$P4_NAME/custom_conf_file"
fi

if [ -z ${TARGET_CONFIG_FILE} ]; then
  if [ -f $CUSTOM_CONF_FILE ]; then
    echo "Detected custom conf file $(<$CUSTOM_CONF_FILE)"
    TARGET_CONFIG_FILE=$SDE_INSTALL/share/p4/targets/$CHIP_ARCH/$(<$CUSTOM_CONF_FILE)
  else
    TARGET_CONFIG_FILE=$SDE_INSTALL/share/p4/targets/$CHIP_ARCH/$P4_NAME.conf
  fi
fi

[ ! -r $TARGET_CONFIG_FILE ] && echo "File $TARGET_CONFIG_FILE not found" && exit 1

echo "Using TARGET_CONFIG_FILE ${TARGET_CONFIG_FILE}"

export PATH=$SDE_INSTALL/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/lib:$SDE_INSTALL/lib:$LD_LIBRARY_PATH

echo "Using PATH ${PATH}"
echo "Using LD_LIBRARY_PATH ${LD_LIBRARY_PATH}"

#Start tofino-driver
if [[ $REDIRECTLOG != "" ]]; then
  sudo env "SDE=$SDE" "SDE_INSTALL=$SDE_INSTALL" $ASAN_ON_ERROR "PATH=$PATH" \
      "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" $DBG bf_switchd \
    "$SERVER_LISTEN_LOCAL_ONLY" \
	--install-dir $SDE_INSTALL --conf-file $TARGET_CONFIG_FILE "--init-mode=$INIT_MODE" \
	$SKIP_HLD_STR $SKIP_P4_STR $SKIP_PORT_ADD_STR $STS_PORT_STR $KERNEL_PKT_STR $SHELL_NO_WAIT $@ &> $REDIRECTLOG &
else
  sudo env "SDE=$SDE" "SDE_INSTALL=$SDE_INSTALL" $ASAN_ON_ERROR "PATH=$PATH" \
      "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" $DBG bf_switchd \
    "$SERVER_LISTEN_LOCAL_ONLY" \
	--install-dir $SDE_INSTALL --conf-file $TARGET_CONFIG_FILE "--init-mode=$INIT_MODE" \
	$SKIP_HLD_STR $SKIP_P4_STR $SKIP_PORT_ADD_STR $STS_PORT_STR $KERNEL_PKT_STR $SHELL_NO_WAIT $@
fi