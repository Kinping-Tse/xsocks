
BUILD_ROOT = ..
ROOT = $(BUILD_ROOT)/..
DEPS_PATH = $(ROOT)/deps
BUILD_DEPS_PATH = $(BUILD_ROOT)/deps
SRC_ROOT = $(ROOT)/src
VPATH = $(SRC_ROOT)/core:$(SRC_ROOT)/event:$(SRC_ROOT)/module

include $(BUILD_ROOT)/common.mk

PREFIX ?= /usr/local
INSTALL_BIN = $(PREFIX)/bin
# INSTALL = install

SRCS = $(wildcard $(SRC_ROOT)/core/*.c $(SRC_ROOT)/event/*.c)
OBJS = $(notdir $(SRCS:.c=.o)) module.o

DEPENDENCY_TARGETS = libev redis json-parser shadowsocks-libev libbloom

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

EVENT = ae
ifeq ($(USE_AE), yes)
	EVENT = ae
endif

ifeq ($(USE_LIBEV), yes)
	EVENT = libev
endif

-include .make-settings

EXT_CFLAGS = -I$(DEPS_PATH) -I$(BUILD_DEPS_PATH)/libsodium/.libsodium/include -I$(BUILD_DEPS_PATH)/mbedtls/include
EXT_LDFLAGS = -L$(BUILD_DEPS_PATH)/libsodium/.libsodium/lib -L$(BUILD_DEPS_PATH)/mbedtls/lib
EXT_LIBS = $(BUILD_DEPS_PATH)/libev/.libs/libev.a \
	$(BUILD_DEPS_PATH)/redis/libredis.a \
	$(BUILD_DEPS_PATH)/json-parser/libjsonparser.a \
	$(BUILD_DEPS_PATH)/shadowsocks-libev/libshadowsocks-libev.a \
	$(DEPS_PATH)/libbloom/build/libbloom.a \
	-lsodium -lmbedcrypto

ifeq ($(EVENT), ae)
	EXT_CFLAGS += -DUSE_AE
endif

ifeq ($(EVENT), libev)
	EXT_CFLAGS += -DUSE_LIBEV
endif

XSOCKS_SERVER_NAME = xs-server
XSOCKS_SERVER_OBJ = server.o $(OBJS)
XSOCKS_LOCAL_NAME = xs-local
XSOCKS_LOCAL_OBJ = local.o $(OBJS)
XSOCKS_TUNNEL_NAME = xs-tunnel
XSOCKS_TUNNEL_OBJ = tunnel.o $(OBJS)

XSOCKS_MODULE_EXE = $(XSOCKS_SERVER_NAME) $(XSOCKS_LOCAL_NAME) $(XSOCKS_TUNNEL_NAME)

all: $(XSOCKS_MODULE_EXE)

ifeq (0, $(words $(findstring $(MAKECMDGOALS), "clean distclean")))
-include Makefile.dep
endif

Makefile.dep:
	$(COMMON_CC) -MM $(SRC_ROOT)/*/*.c > Makefile.dep

persist-settings: distclean
	echo STD=$(STD) >> .make-settings
	echo WARN=$(WARN) >> .make-settings
	echo OPT=$(OPT) >> .make-settings
	echo MALLOC=$(MALLOC) >> .make-settings
	echo EVENT=$(EVENT) >> .make-settings
	echo CFLAGS=$(CFLAGS) >> .make-settings
	echo LDFLAGS=$(LDFLAGS) >> .make-settings
	echo EXT_CFLAGS=$(EXT_CFLAGS) >> .make-settings
	echo EXT_LDFLAGS=$(EXT_LDFLAGS) >> .make-settings
	echo PREV_FINAL_CFLAGS=$(FINAL_CFLAGS) >> .make-settings
	echo PREV_FINAL_LDFLAGS=$(FINAL_LDFLAGS) >> .make-settings
	cd $(BUILD_ROOT)/deps && $(MAKE) $(MFLAGS) $(DEPENDENCY_TARGETS)

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

%.o: %.c .make-prerequisites
	$(COMMON_CC) -c $<

clean:
	rm -rf $(XSOCKS_MODULE_EXE) *.o Makefile.dep

distclean: clean
	cd $(BUILD_ROOT)/deps && $(MAKE) distclean
	rm -rf .make-*

.PHONY: all clean distclean persist-settings
