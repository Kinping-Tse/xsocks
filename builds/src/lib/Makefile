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

ROOT = $(realpath ../../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

SRC_ROOT = $(ROOT)/src/lib
VPATH = $(wildcard $(SRC_ROOT)/*)

SRCS = $(wildcard $(SRC_ROOT)/*/*.c)
OBJS = $(notdir $(SRCS:.c=.o))
LIBNAME = libxsocks

include $(ROOT)/builds/lib.mk

EXT_CFLAGS += -I$(DEPS_PATH) $(LIBSODIUM_HEADER_CFLAGS) -I$(MBEDTLS_PATH)/include
EXT_LDFLAGS += -L$(REDIS_PATH) -L$(SHADOWSOCKS_LIBEV_PATH) -L$(JSONPARSER_PATH)
EXT_LIBS += -lredis -lshadowsocks-libev -ljsonparser

ifeq ($(EVENT), ae)
	EXT_CFLAGS += -DUSE_AE
endif
ifeq ($(EVENT), libev)
	EXT_CFLAGS += -DUSE_LIBEV -Wno-strict-aliasing
	EXT_LDFLAGS += -L$(LIBEV_PATH)/.libs
	EXT_LIBS += -lev
endif

ifeq ($(MALLOC), jemalloc)
	EXT_CFLAGS += -DUSE_JEMALLOC -I$(JEMALLOC_PATH)/include
endif
