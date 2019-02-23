#!/bin/sh

# See https://github.com/RMerl/am-toolchains

TOOLCHAIN_BASE=/opt/toolchains
ARM_CROSSTOOLS=$TOOLCHAIN_BASE/crosstools-arm-gcc-5.3-linux-4.1-glibc-2.22-binutils-2.25
AARCH64_CROSSTOOLS=$TOOLCHAIN_BASE/crosstools-aarch64-gcc-5.3-linux-4.1-glibc-2.22-binutils-2.25

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ARM_CROSSTOOLS/usr/lib
export TOOLCHAIN_BASE=$TOOLCHAIN_BASE
export PATH=$PATH:$ARM_CROSSTOOLS/usr/bin
export PATH=$PATH:$AARCH64_CROSSTOOLS/usr/bin

ln -sf /tmp/am-toolchains/brcm-arm-hnd $TOOLCHAIN_BASE

make -C $ROOT distclean
make -C $ROOT -j $(nproc) HOST=$HOST OPTIMIZATION=-O3 DEBUG= USE_JEMALLOC=no USE_LIBEV=no USE_SHARED=no
make -C $ROOT install USE_STRIP=yes
