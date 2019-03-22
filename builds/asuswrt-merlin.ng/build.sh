#!/bin/sh

#
# This file is part of xsocks, a lightweight proxy tool for science online.
#
# Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

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
make -C $ROOT -j $(nproc) HOST=$HOST OPTIMIZATION=-O3 DEBUG= USE_JEMALLOC=no USE_LIBEV=yes USE_SHARED=no
make -C $ROOT install USE_STRIP=yes
