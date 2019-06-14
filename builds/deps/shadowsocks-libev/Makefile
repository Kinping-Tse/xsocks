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

VPATH = $(DEPS_PATH)/shadowsocks-libev

SRCS = $(wildcard $(VPATH)/*.c)
OBJS = $(notdir $(SRCS:.c=.o))
LIBNAME = libshadowsocks-libev

include $(ROOT)/builds/lib.mk

WARN = -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter
EXT_CFLAGS += -I$(DEPS_PATH)/libev -I$(DEPS_PATH)/libbloom -I$(MBEDTLS_PATH)/include $(LIBSODIUM_HEADER_CFLAGS) $(LIBCORK_HEADER_CFLAGS) $(LIBIPSET_HEADER_CFLAGS) -DHAVE_PCRE_H
EXT_LDFLAGS += -L$(LIBEV_PATH)/.libs -L$(LIBBLOOM_PATH) -L$(MBEDTLS_PATH)/library -L$(LIBSODIUM_PATH)/.libs $(LIBCORK_LIB_LDFLAGS) -L$(LIBIPSET_PATH)
EXT_LIBS += -lev -lbloom -lmbedcrypto -lsodium -lcork -lipset -lpcre
