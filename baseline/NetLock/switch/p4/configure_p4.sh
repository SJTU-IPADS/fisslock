#!/bin/bash

$SDE/pkgsrc/p4-build/configure \
    --with-tofino \
    --with-p4c=bf-p4c \
    --prefix=$SDE_INSTALL \
    --bindir=$SDE_INSTALL/bin \
    P4_NAME=netlock \
    P4_PATH=/home/ck/workspace/netlock_reproduce/p4/netlock.p4 \
    P4_VERSION=p4-16 \
    P4_ARCHITECTURE=tna \
    LDFLAGS="-L$SDE_INSTALL/lib"
