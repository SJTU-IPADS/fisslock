#!/bin/bash

$SDE/pkgsrc/p4-build/configure \
    --with-tofino \
    --with-p4c=bf-p4c \
    --prefix=$SDE_INSTALL \
    --bindir=$SDE_INSTALL/bin \
    P4_NAME=$1 \
    P4_PATH=$2 \
    P4_VERSION=p4-16 \
    P4_ARCHITECTURE=tna \
    LDFLAGS="-L$SDE_INSTALL/lib"
