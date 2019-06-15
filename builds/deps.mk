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

VERSION = $(shell grep XS_VERSION $(ROOT)/src/lib/core/version.h | awk -F \" '{print $$2}')

DEPENDENCY_TARGETS = redis json-parser shadowsocks-libev

MALLOC = libc
ifeq ($(USE_TCMALLOC), yes)
	MALLOC = tcmalloc
endif
ifeq ($(USE_TCMALLOC_MINIMAL), yes)
	MALLOC = tcmalloc_minimal
endif
ifeq ($(USE_JEMALLOC), yes)
	MALLOC = jemalloc
endif

EVENT = libev
ifeq ($(USE_AE), yes)
	EVENT = ae
endif
ifeq ($(USE_LIBEV), yes)
	EVENT = libev
endif

PREFIX ?= $(ROOT)/tmp
LIBRARY_PATH ?= lib
INSTALL_BIN = $(PREFIX)/bin
INSTALL_INC = $(PREFIX)/include
INSTALL_LIB = $(PREFIX)/$(LIBRARY_PATH)
INSTALL_ETC = $(PREFIX)/etc
INSTALL_DATA = $(PREFIX)/share

DEPS_PATH = $(ROOT)/deps
BUILD_ROOT = $(ROOT)/builds
BUILD_DEPS_PATH = $(BUILD_ROOT)/deps

JEMALLOC_PATH = $(BUILD_DEPS_PATH)/jemalloc
JEMALLOC_SRC_PATH = $(DEPS_PATH)/jemalloc

LIBEV_PATH = $(BUILD_DEPS_PATH)/libev
REDIS_PATH = $(BUILD_DEPS_PATH)/redis
SHADOWSOCKS_LIBEV_PATH = $(BUILD_DEPS_PATH)/shadowsocks-libev
JSONPARSER_PATH = $(BUILD_DEPS_PATH)/json-parser
MBEDTLS_PATH = $(DEPS_PATH)/mbedtls

LIBBLOOM_PATH = $(BUILD_DEPS_PATH)/libbloom
LIBBLOOM_SRC_PATH = $(DEPS_PATH)/libbloom

LIBSODIUM_PATH = $(BUILD_DEPS_PATH)/libsodium/src/libsodium
LIBSODIUM_SRC_PATH = $(DEPS_PATH)/libsodium
LIBSODIUM_HEADER_CFLAGS = -I$(LIBSODIUM_SRC_PATH)/src/libsodium/include \
	-I$(LIBSODIUM_SRC_PATH)/src/libsodium/include/sodium \
	-I$(LIBSODIUM_PATH)/include

LIBCORK_PATH = $(BUILD_DEPS_PATH)/libcork
LIBCORK_SRC_PATH = $(DEPS_PATH)/libcork
LIBCORK_HEADER_CFLAGS = -I$(LIBCORK_SRC_PATH)/include -I$(LIBCORK_PATH)/include
LIBCORK_LIB_LDFLAGS = -L$(LIBCORK_PATH)/.libs

LIBIPSET_PATH = $(BUILD_DEPS_PATH)/libipset
LIBIPSET_SRC_PATH = $(DEPS_PATH)/libipset
LIBIPSET_HEADER_CFLAGS = -I$(LIBIPSET_SRC_PATH)/include

PCRE_PATH = $(BUILD_DEPS_PATH)/pcre
PCRE_SRC_PATH = $(DEPS_PATH)/pcre
PCRE_LIB_PATH = $(PCRE_PATH)/.libs
