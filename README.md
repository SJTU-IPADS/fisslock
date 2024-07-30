
# FissLock - Fast and Scalable In-network Lock Service

FissLock is a fast and scalable distributed lock service that exploits the programmable 
switch to improve latency and peak throughput for managing millions of locks.

## Table of Contents

- [Introduction](#introduction)
- [Requirements](#requirements)
- [Configure and Build](#configure-and-build)
- [Reproduce Experiment Results](#reproduce-experiment-results)

## Introduction

FissLock is a distributed lock service that meets the following design goals:
- **Efficiency**: Grant locks in single-digit microseconds.
- **Pervasiveness**: Unleash full-scale acceleration for millions of locks.
- **Robustness**: Ensure stable performance for diverse workloads without prior knowledge.

FissLock achieves the above goals by adopting a novel lock scheme called __*lock fission*__.
It decouples the grant decision and metadata maintenance phases in lock management in 
terms of both functionality and metadata. Thereby, FissLock fully exploits the high packet
processing power of programmable switches with minimal requirement of switch memory, leading
to efficient management of million-scale locks. 

More details can be found in our paper:
[Fast and Scalable In-network Lock Management Using Lock Fission (OSDI'24)](https://ipads.se.sjtu.edu.cn/_media/publications/fisslock-osdi24.pdf).

If you intend to use FissLock in your research, please cite our paper:

```bibtex
@inproceedings {osdi2024fisslock,
  author = {Hanze Zhang and Ke Cheng and Rong Chen and Haibo Chen},
  title = {Fast and Scalable In-network Lock Management Using Lock Fission},
  booktitle = {18th USENIX Symposium on Operating Systems Design and Implementation (OSDI 24)},
  year = {2024},
  address = {Santa Clara, CA},
  publisher = {USENIX Association},
  month = jul,
}
```

## Requirements

### Hardware Requirements

The on-switch part of FissLock is implemented atop 
[Intel Tofino ASIC](https://www.intel.com/content/www/us/en/products/details/network-io/intelligent-fabric-processors/tofino.html) 
using [P4 language](https://en.wikipedia.org/wiki/P4_(programming_language)).
While we use a **Wedge100BF-32X** programmable switch during experiments, other switches
equipped with Intel Tofino ASICs are also applicable.

### Software Requirements

#### Barefoot SDE

> The SDE is only required on the switch control plane, not on servers.

The development of the switch data plane and control plane relies on Barefoot SDE,
a development environment provided by switch manufacturers.
The version of the SDE we use is `9.5.2`.

#### DPDK

By default, FissLock sends and receives packets via DPDK. We use `dpdk-21.11.2`
during the development of the project. The source code can be downloaded from
[here](https://git.dpdk.org/dpdk-stable/snapshot/dpdk-stable-21.11.2.tar.gz).

In the decompressed project source folder, execute:

```bash
meson build --prefix=<xxx>
cd build && ninja && ninja install
ldconfig
```

to configure and build DPDK. `<xxx>` denotes the installation path,
which should typically be `.local/` in the user's home directory.
You may also run `meson build` without the `--prefix` option to install
globally, for which `sudo` is required when installing.

If you have not added `.local` to environments, add the following lines 
to your `<HOME_PATH>/.bashrc`:

```bash
export PATH=$PATH:$HOME/.local/bin
export LD_LIBRARY_PATH=$HOME/.local/lib:$HOME/.local/lib/x86_64-linux-gnu
export LIBRARY_PATH=$HOME/.local/lib:$HOME/.local/lib/x86_64-linux-gnu
export CPATH=$HOME/.local/include
export PKG_CONFIG_PATH=$HOME/.local/lib/pkgconfig:$HOME/.local/lib/x86_64-linux-gnu/pkgconfig
```

then run `source .bashrc`.

#### R2 and rlib

R2 and rlib are utility libraries for high-performance network 
programming developed by Xingda Wei et al. from [IPADS](https://ipads.se.sjtu.edu.cn/). 
FissLock uses the coroutine implementation in R2 to host multiple 
clients on each CPU core.

After cloning the FissLock repository, run:

```bash
git submodule update --init --recursive
```

to download all submodules, including R2 and rlib.
Afterwards, enter `vendor/r2` , open `CMakeLists.txt`, and modify

```bash
ADD_DEFINITIONS(-std=c++14)
```

to

```bash
ADD_DEFINITIONS(-std=c++17)
```

Then execute:

```bash
mkdir build && cd build/
cmake .. && make boost && make r2
mv libr2.a ../
```

to build and install the R2 library.

## Configure and Build

### Project Structure

```
fisslock
│ 
├── baseline        # Baselines:
│   ├── NetLock     #     NetLock and SrvLock
│   └── ParLock     #     ParLock
│ 
├── experiments     # Scripts for experiments:
│   ├── exec        #     execute or terminate clients
│   ├── fault       #     simulate hardware errors
│   ├── netlock     #     static analysis for NetLock
│   ├── results     #     collect and parse results
│   └── trace_gen   #     generate traces of workloads
│ 
├── switch          # Switch-side implementation:
│   ├── control     #     control plane (C++)
│   └── p4          #     data plane (P4)
│ 
├── include         # Headers
├── lib             # FissLock library
├── utils           # Utility library
├── tests           # Test drivers and benchmarks
└── vendor          # Dependencies
```

### Server Part

In the project root, run:

```bash
make SYSTEM=xxx
```

to generate the test driver binaries, where `xxx` is the system name 
(`fisslock`, `parlock`, `srvlock`, or `netlock`).
Generated binaries are put in the `build/` folder.

### Switch Part

Before compiling the switch data plane and control plane programs, execute:

```bash
cd /path/to/bf-sde-9.5.2
source set_sde.bash
```

to set up the SDE.

#### Data plane

Create an empty folder (e.g., `fisslock-p4-build`), and build the data plane by
exeucting the following commands:

```bash
cd fisslock-p4-build
bash /path/to/fisslock/switch/configure_p4.sh \
     fisslock_decider /path/to/fisslock/switch/p4/switch.p4
make && make install
```

where `/path/to/fisslock` denotes the project root path of FissLock.

> The "make" process may take a while (~1min), please wait with patience.

#### Control plane

For convenience, we hardcode the network connection configurations in 
`switch/control/cluster.h`. Please edit the file according to the
setup of your own cluster.

```C
// Hostname, IP address, MAC address, Connected switch port 
  {"pro0-1", 0x0a000204, 0x1070fd0de230, 24}
```

> The "connected switch port" field in `cluster.h` can be acquired through
> `bfshell`. In the `ucli` mode of `bfshell`, `pm show -a` displays all
> available ports on the switch, where the `D_P` column denotes the port
> serials that are used in `cluster.h`.

We reuse the configuration and Makefile templates in the SDE to build the control plane.
To achieve this, first move the control plane source code to bfrt driver path:

```bash
cd $SDE/pkgsrc/bf-drivers/
cp -r /path/to/fisslock/switch/control ./fisslock
```

Edit the `Makefile.am` and `configure.ac` files in the `bf-drivers` directory, add 
fisslock to `SUBDIRS` and `AC_CONFIG_FILES`:

```makefile
# [[Makefile.am]]
# ...
SUBDIRS += fisslock

# [[configure.ac]]
# ...
# AC_CONFIG_FILES([Makefile
# ...
fisslock/Makefile
# ])
```

Then, execute the following commands in the `bf-drivers` directory:

```bash
# Generate configuration scripts
autoreconf

# Generate Makefile
export PKG_CONFIG_PATH=$SDE_INSTALL/lib/pkgconfig/
./configure --prefix=$SDE_INSTALL --enable-grpc --enable-thrift --host=x86_64-linux-gnu

# Build the control plane
cd fisslock
make && make install
```

## Reproduce Experiment Results

We have automized most experiment procedures to enable push-button experiments.
However, the experiment framework needs to be configured according to the specific
hardware in the cluster.
All scripts for experiments are put in the `experiments/` folder. We will introduce
the steps to reproduce experiment results in the paper step by step.

### Setup the Environment

To set up the experiment environment, first edit the `experiments/set-env.sh` script
according to the realistic state of your cluster. You also need to edit the 
cluster-specific part of `experiments/run-on-all.sh` and modify the `<hostname>` 
and `<nic_id>` fields. The `<nic_id>` indicates the PCI device ID of NICs, which
can be queried by `lspci | grep "Ethernet controller"`.

> The `set-env.sh` script needs to be sourced throughout the experiment 
(run `source experiments/set-env.sh` before executing other steps). 

### Generate Traces

The benchmark traces are generated by programs and scripts in `experiments/trace_gen`.
To generate traces for a specific test, just run:

```bash
make -C experiments/trace_gen <testname>_trace
```

where `<testname>` denotes the test name, including `micro`, `tatp`, `tpcc`, `dynamic`, 
and `lkscale`. The generated traces reside in `$FISSLOCK_TRACE_PATH` in `set-env.sh`.

### Generate Analysis Results for NetLock (Optional)

> This step can be skipped if you do not want to reproduce NetLock's results.

NetLock require static analysis of the workload to determine the size of
lock queues on the switch.
To generate the analysis results of NetLock's knapsack algorithm, run:

```bash
make -C experiments/netlock <testname>_trace
```

where `<testname>` denotes the test name, including `ycsb`, `tatp`, `tpcc`, `dynamic`, 
and `lkscale`. The generated results reside in `$FISSLOCK_PATH/build/netlock_len_in_switch`,
`$FISSLOCK_PATH/build/netlock_map` and `$FISSLOCK_PATH/build/netlock_lock_freq`.

### Synchronize to Worker Machines

Run `experiments/sync-env.sh`
to synchronize all scripts, traces, and binaries to worker machines in the cluster.

> **Note**: Whenever the binaries are re-compiled, or the traces are updated, the
> `experiment/sync-env.sh` should be executed to synchronize changes.

### Launch the Switch Program

> Do not forget to execute `source set_sde.bash` before this step!

To launch the decider of FissLock, run the following command in `$FISSLOCK_PATH`:

```bash
bash switch/run_decider.sh -p fisslock_decider -s switch/port-setup.bfsh
```

> The decider of FissLock also provides the packet forwarding functionality 
> for ParLock. Thus, when testing ParLock, the switch program is launched
> in the same way.

To launch the lock switch program of NetLock, run the following command 
in `$FISSLOCK_PATH`:

```bash
bash switch/run_lockmgr.sh -p <netlock|central_srv>
```

> The lock switch of NetLock also provides the packet forwarding functionality 
> for SrvLock. Thus, when testing SrvLock, the switch program is launched
> in the same way.

### Run the Experiments

We have prepared an automatic script for running all experiments:

```bash
experiments/evaluation.sh <system_name> <benchmark> <think_time> <crt_num>
```

The `system_name` can be `fisslock`, `parlock`, `netlock` and `srvlock`.
The `think_time` and `crt_num` of each `benchmark` are:

|`benchmark`|`think_time`|`crt_num`|
|:--:|:--:|:--:|
|`micro-<uh\|rm\|ro>-<zipf\|uni>`|0|4|
|`tpcc`|10|30|
|`tatp`|1|4|
|`dynamic`|0|4|
|`lkscale`|0|4|

This script runs the benchmark on the cluster
and collects the results after the experiment finishes.

### Process Results

To parse the experiment results and generate source data for plotting, execute:

```bash
make -C experiments/results SYSTEM=xxx TEST=xxx
```

where `SYSTEM` can be `fisslock`, `parlock`, `netlock` and `srvlock`, and
`TEST` is the experiment name (e.g., `micro-rm-zipf`).
