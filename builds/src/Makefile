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

ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

SRC_ROOT = $(ROOT)/src
APP_SRC_ROOT = $(SRC_ROOT)/app

VPATH = $(APP_SRC_ROOT) $(wildcard $(APP_SRC_ROOT)/*)
SRCS = $(wildcard $(APP_SRC_ROOT)/*/*c)
OBJS = $(notdir $(SRCS:.c=.o))

LINK_MODE = static
ifeq ($(USE_SHARED), yes)
	LINK_MODE = shared
endif

-include .make-settings

EXT_CFLAGS = -I$(SRC_ROOT) -I$(DEPS_PATH) $(LIBSODIUM_HEADER_CFLAGS) -I$(MBEDTLS_PATH)/include
EXT_LDFLAGS = -Llib -L$(REDIS_PATH) -L$(SHADOWSOCKS_LIBEV_PATH)
EXT_LIBS = -lxsocks -lredis -lshadowsocks-libev

ifeq ($(LINK_MODE), static)
	EXT_LDFLAGS += -L$(LIBBLOOM_PATH) -L$(MBEDTLS_PATH)/library -L$(LIBSODIUM_PATH)/.libs $(LIBCORK_LIB_LDFLAGS) -L$(LIBIPSET_PATH) -L$(PCRE_LIB_PATH) -L$(JSONPARSER_PATH)
	EXT_LIBS = -Wl,-static -lxsocks -lredis -lshadowsocks-libev -lbloom -lmbedcrypto -lsodium -lipset -lcork -lpcre -ljsonparser
	ifeq ($(MALLOC), jemalloc)
		EXT_LDFLAGS += -L$(JEMALLOC_PATH)/lib
		EXT_LIBS += -ljemalloc -pthread
	endif
	ifeq ($(EVENT), libev)
		EXT_LDFLAGS += -L$(LIBEV_PATH)/.libs
		EXT_LIBS += -lev
	endif

	ifeq ($(uname_S), Linux)
		EXT_LIBS += -Wl,-dy -lm
	endif
	ifeq ($(uname_S), Darwin)
		EXT_LIBS += -Wl,-dylib -Wl,-execute
	endif
endif

ifeq ($(LINK_MODE), shared)
	ifeq ($(uname_S), Linux)
		EXT_LDFLAGS += -Wl,-rpath,$(INSTALL_LIB),-rpath,$(BUILD_ROOT)/src/lib,-rpath,$(REDIS_PATH),-rpath,$(SHADOWSOCKS_LIBEV_PATH),-rpath,$(JSONPARSER_PATH),-rpath,$(LIBBLOOM_PATH),-rpath,$(MBEDTLS_PATH)/library,-rpath,$(LIBSODIUM_PATH)/.libs,-rpath,$(LIBEV_PATH)/.libs,-rpath,$(LIBCORK_PATH)/.libs,-rpath,$(LIBIPSET_PATH),-rpath,$(PCRE_LIB_PATH)
		ifeq ($(MALLOC), jemalloc)
			EXT_LDFLAGS += -Wl,-rpath,$(JEMALLOC_PATH)/lib
		endif
	endif
endif

ifeq ($(uname_S), Linux)
	EXT_LIBS += -ldl
endif

XSOCKS_SERVER_NAME = xs-server
XSOCKS_SERVER_OBJ = server.o $(OBJS)
XSOCKS_LOCAL_NAME = xs-local
XSOCKS_LOCAL_OBJ = local.o $(OBJS)
XSOCKS_TUNNEL_NAME = xs-tunnel
XSOCKS_TUNNEL_OBJ = tunnel.o $(OBJS)
XSOCKS_REDIR_NAME = xs-redir
XSOCKS_REDIR_OBJ = redir.o $(OBJS)
XSOCKS_BENCHMAKR_SERVER_NAME = xs-benchmark-server
XSOCKS_BENCHMAKR_SERVER_OBJ = benchmark_server.o
XSOCKS_BENCHMAKR_CLIENT_NAME = xs-benchmark-client
XSOCKS_BENCHMAKR_CLIENT_OBJ = benchmark_client.o

XSOCKS_MODULE_EXE = $(XSOCKS_SERVER_NAME) $(XSOCKS_LOCAL_NAME) $(XSOCKS_TUNNEL_NAME)
XSOCKS_BENCHMAKR_EXE = $(XSOCKS_BENCHMAKR_SERVER_NAME) $(XSOCKS_BENCHMAKR_CLIENT_NAME)

ifeq ($(uname_S), Linux)
XSOCKS_MODULE_EXE += $(XSOCKS_REDIR_NAME)
endif

all: $(XSOCKS_MODULE_EXE)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), "clean distclean")))
-include Makefile.dep
endif

lib: .make-prerequisites
	$(MAKE) -C $@ $(MFLAGS)

Makefile.dep:
	$(COMMON_CC) -MM $(SRC_ROOT)/*/*.c > Makefile.dep 2> /dev/null

persist-settings: distclean
	@printf "Set env to to %b\n" $(COLOR_INFO)$(BUILD_ROOT)/src/.make-settings$(ENDCOLOR)
	@echo HOST=$(HOST) >> .make-settings
	@echo STD=$(STD) >> .make-settings
	@echo WARN=$(WARN) >> .make-settings
	@echo OPT=$(OPT) >> .make-settings
	@echo DEBUG=$(DEBUG) >> .make-settings
	@echo CFLAGS=$(CFLAGS) >> .make-settings
	@echo LDFLAGS=$(LDFLAGS) >> .make-settings
	@echo EXT_CFLAGS=$(EXT_CFLAGS) >> .make-settings
	@echo EXT_LDFLAGS=$(EXT_LDFLAGS) >> .make-settings
	@echo PREFIX=$(PREFIX) >> .make-settings
	@echo MALLOC=$(MALLOC) >> .make-settings
	@echo EVENT=$(EVENT) >> .make-settings
	@echo LINK_MODE=$(LINK_MODE) >> .make-settings
	@echo PREV_FINAL_CFLAGS=$(FINAL_CFLAGS) >> .make-settings
	@echo PREV_FINAL_LDFLAGS=$(FINAL_LDFLAGS) >> .make-settings
	cd $(BUILD_DEPS_PATH) && $(MAKE) deps-init && $(MAKE) $(MFLAGS) $(DEPENDENCY_TARGETS)

.make-prerequisites:
	@touch $@

ifneq ($(strip $(PREV_FINAL_CFLAGS)), $(strip $(FINAL_CFLAGS)))
.make-prerequisites: persist-settings
endif

ifneq ($(strip $(PREV_FINAL_LDFLAGS)), $(strip $(FINAL_LDFLAGS)))
.make-prerequisites: persist-settings
endif

XSOCKS_MODULE_EXE_LD = $(COMMON_LD) -o $@ $^ $(FINAL_LIBS)

$(XSOCKS_SERVER_NAME): $(XSOCKS_SERVER_OBJ)
	$(XSOCKS_MODULE_EXE_LD)

$(XSOCKS_LOCAL_NAME): $(XSOCKS_LOCAL_OBJ)
	$(XSOCKS_MODULE_EXE_LD)

$(XSOCKS_TUNNEL_NAME): $(XSOCKS_TUNNEL_OBJ)
	$(XSOCKS_MODULE_EXE_LD)

$(XSOCKS_REDIR_NAME): $(XSOCKS_REDIR_OBJ)
	$(XSOCKS_MODULE_EXE_LD)

$(XSOCKS_BENCHMAKR_CLIENT_NAME): $(XSOCKS_BENCHMAKR_CLIENT_OBJ)
	$(XSOCKS_MODULE_EXE_LD)

$(XSOCKS_BENCHMAKR_SERVER_NAME): $(XSOCKS_BENCHMAKR_SERVER_OBJ)
	$(XSOCKS_MODULE_EXE_LD)

%.o: %.c lib
	$(COMMON_CC) -c $<

install: all install-lib install-data
	@mkdir -p $(INSTALL_BIN) $(INSTALL_ETC)
	$(COMMON_INSTALL) $(XSOCKS_MODULE_EXE) $(INSTALL_BIN)
	$(COMMON_INSTALL) $(ROOT)/config.json $(INSTALL_ETC)
	@printf "\nInstall to %b\n\n" $(COLOR_INFO)$(PREFIX)$(ENDCOLOR)
ifeq ($(USE_STRIP), yes)
	$(STRIP) $(INSTALL_BIN)/*
endif

install-lib:

ifeq ($(LINK_MODE), shared)
install-lib:
	@mkdir -p $(INSTALL_LIB)
	$(MAKE) -C lib install-shared PREFIX="$(PREFIX)"
	$(MAKE) -C $(BUILD_DEPS_PATH) install-shared PREFIX="$(PREFIX)"
ifeq ($(USE_STRIP), yes)
	$(STRIP) $(INSTALL_LIB)/*
endif
endif

install-data:
	@mkdir -p $(INSTALL_DATA)
	cp -a $(ROOT)/share/* $(INSTALL_DATA)

clean:
	rm -rf $(XSOCKS_MODULE_EXE) $(BUILD_TMP_FILES) *.info lcov-html Makefile.dep $(XSOCKS_BENCHMAKR_EXE)
	$(MAKE) -C lib clean

distclean: clean
	$(MAKE) -C $(BUILD_DEPS_PATH) distclean
	rm -rf .make-*

bench: $(XSOCKS_BENCHMAKR_EXE)

gcov:
	$(MAKE) $(MFLAGS) all bench XS_CFLAGS="--coverage" XS_LDFLAGS="--coverage"

lcov:
	$(MAKE) $(MFLAGS) gcov
	@geninfo -o xsocks.info .
	@genhtml --legend -o lcov-html xsocks.info

valgrind:
	$(MAKE) $(MFLAGS) OPTIMIZATION="-O0" MALLOC="libc"

.PHONY: all lib install install-lib clean distclean persist-settings bench gcov lcov valgrind
