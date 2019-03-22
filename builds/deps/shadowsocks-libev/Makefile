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
DYLIBNAME = $(LIBNAME).$(DYLIB_SUFFIX)
STLIBNAME = $(LIBNAME).$(STLIB_SUFFIX)

WARN = -Wall -Wextra #-pedantic
EXT_CFLAGS = -fPIC -I$(DEPS_PATH)/libbloom -I$(MBEDTLS_PATH)/include $(LIBSODIUM_HEADER_CFLAGS)
EXT_LDFLAGS = -shared -L$(LIBBLOOM_PATH) -L$(MBEDTLS_PATH)/library -L$(LIBSODIUM_PATH)/.libs
EXT_LIBS += -lbloom -lmbedcrypto -lsodium
ARFLAGS =
EXT_ARFLAGS = -rcs

DYLIB_MAKE_CMD = $(COMMON_LD)
ifeq ($(uname_S), Darwin)
DYLIB_MAKE_CMD = $(COMMON_LD) -Wl,-install_name,$(PREFIX)/lib/$(DYLIBNAME)
endif

all: dynamic static
dynamic: $(DYLIBNAME)
static: $(STLIBNAME)

install-shared: $(DYLIBNAME)
	$(COMMON_INSTALL) -d $(INSTALL_LIB)
	$(COMMON_INSTALL) -m 755 $(DYLIBNAME) $(INSTALL_LIB)

$(DYLIBNAME): $(OBJS)
	$(DYLIB_MAKE_CMD) -o $@ $^ $(FINAL_LIBS)

$(STLIBNAME): $(OBJS)
	$(COMMON_AR) $@ $^

%.o: %.c
	$(COMMON_CC) -c $<

clean:
	rm -rf $(DYLIBNAME) $(STLIBNAME) $(BUILD_TMP_FILES)

.PHONY: all install-shared dynamic static clean
