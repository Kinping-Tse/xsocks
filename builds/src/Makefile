
ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

SRC_ROOT = $(ROOT)/src
VPATH = $(SRC_ROOT)/core:$(SRC_ROOT)/event:$(SRC_ROOT)/module

include $(BUILD_ROOT)/common.mk

PREFIX ?= $(ROOT)/tmp
INSTALL_BIN = $(PREFIX)/bin
INSTALL_LIB = $(PREFIX)/lib
INSTALL_ETC = $(PREFIX)/etc

SRCS = $(wildcard $(SRC_ROOT)/core/*.c $(SRC_ROOT)/event/*.c)
OBJS = $(notdir $(SRCS:.c=.o)) module.o

DEPENDENCY_TARGETS = redis json-parser shadowsocks-libev

EVENT = ae
ifeq ($(USE_AE), yes)
	EVENT = ae
endif
ifeq ($(USE_LIBEV), yes)
	EVENT = libev
endif

LINK_MODE = static
ifeq ($(USE_SHARED), yes)
	LINK_MODE = shared
endif

-include .make-settings

EXT_CFLAGS = -I$(DEPS_PATH) -I$(LIBSODIUM_PATH)/include -I$(MBEDTLS_PATH)/include
EXT_LDFLAGS =
EXT_LIBS = -lm

ifeq ($(LINK_MODE), static)
EXT_LIBS += $(REDIS_PATH)/libredis.a \
	$(SHADOWSOCKS_LIBEV_PATH)/libshadowsocks-libev.a \
	$(JSONPARSER_PATH)/lib/libjsonparser.a \
	$(LIBBLOOM_PATH)/libbloom.a \
	$(MBEDTLS_PATH)/lib/libmbedcrypto.a \
	$(LIBSODIUM_PATH)/lib/libsodium.a
	ifeq ($(MALLOC), jemalloc)
		EXT_LIBS += $(JEMALLOC_PATH)/lib/libjemalloc.a -pthread -ldl
	endif
endif
ifeq ($(LINK_MODE), shared)
EXT_LDFLAGS += -L$(REDIS_PATH) \
	-L$(SHADOWSOCKS_LIBEV_PATH) \
	-L$(JSONPARSER_PATH)/lib \
	-Wl,-rpath,$(PREFIX)/lib,-rpath,$(REDIS_PATH),-rpath,$(SHADOWSOCKS_LIBEV_PATH),-rpath,$(JSONPARSER_PATH)/lib
EXT_LIBS += -lredis -ljsonparser -lshadowsocks-libev
endif

ifeq ($(EVENT), ae)
	EXT_CFLAGS += -DUSE_AE
endif
ifeq ($(EVENT), libev)
	EXT_CFLAGS += -DUSE_LIBEV -Wno-strict-aliasing
	DEPENDENCY_TARGETS += libev
	ifeq ($(LINK_MODE), static)
		EXT_LIBS += $(LIBEV_PATH)/libev.a
	endif
	ifeq ($(LINK_MODE), shared)
		EXT_LDFLAGS += -L$(LIBEV_PATH)
		EXT_LIBS += -lev
	endif
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
	echo LINK_MODE=$(LINK_MODE) >> .make-settings
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

install: all install-lib
	@mkdir -p $(INSTALL_BIN) $(INSTALL_ETC)
	$(COMMON_INSTALL) $(XSOCKS_MODULE_EXE) $(INSTALL_BIN)
	$(COMMON_INSTALL) $(ROOT)/config.json $(INSTALL_ETC)

install-lib:

ifeq ($(LINK_MODE), shared)
install-lib:
	@mkdir -p $(INSTALL_LIB)
	$(COMMON_INSTALL) $(JEMALLOC_PATH)/lib/libjemalloc.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	$(COMMON_INSTALL) $(LIBEV_PATH)/libev.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	$(COMMON_INSTALL) $(REDIS_PATH)/libredis.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	$(COMMON_INSTALL) $(SHADOWSOCKS_LIBEV_PATH)/libshadowsocks-libev.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	$(COMMON_INSTALL) $(JSONPARSER_PATH)/libjsonparser.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	$(COMMON_INSTALL) $(MBEDTLS_PATH)/mbedtls/lib/libmbedcrypto.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	$(COMMON_INSTALL) $(LIBBLOOM_PATH)/lib/libbloom.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	$(COMMON_INSTALL) $(LIBSODIUM_PATH)/lib/libsodium.$(DYLIB_SUFFIX) $(INSTALL_LIB)
endif

clean:
	rm -rf $(XSOCKS_MODULE_EXE) *.o Makefile.dep

distclean: clean
	cd $(BUILD_ROOT)/deps && $(MAKE) distclean &> /dev/null
	rm -rf .make-*

.PHONY: all install install-lib clean distclean persist-settings
