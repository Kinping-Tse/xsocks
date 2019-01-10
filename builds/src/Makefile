
BUILD_ROOT = ..
ROOT = $(BUILD_ROOT)/..
DEPS_PATH = $(ROOT)/deps
BUILD_DEPS_PATH = $(BUILD_ROOT)/deps

include $(BUILD_ROOT)/common.mk

PREFIX ?= /usr/local
INSTALL_BIN = $(PREFIX)/bin
# INSTALL = install

DEPENDENCY_TARGETS = libev redis json-parser

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

-include .make-settings

EXT_CFLAGS = -I$(DEPS_PATH)/libev -I$(DEPS_PATH)/redis -I$(DEPS_PATH)/json-parser
EXT_LIBS = $(BUILD_DEPS_PATH)/libev/.libs/libev.a \
	$(BUILD_DEPS_PATH)/redis/libredis.a \
	$(DEPS_PATH)/json-parser/libjsonparser.a

XSOCKS_SERVER_NAME = xs-server
XSOCKS_SERVER_OBJ = server.o
XSOCKS_CLIENT_NAME = xs-client
XSOCKS_CLIENT_OBJ = client.o utils.o config.o logger.o

all: $(XSOCKS_SERVER_NAME) $(XSOCKS_CLIENT_NAME)

persist-settings: distclean
	echo STD=$(STD) >> .make-settings
	echo WARN=$(WARN) >> .make-settings
	echo OPT=$(OPT) >> .make-settings
	echo MALLOC=$(MALLOC) >> .make-settings
	echo CFLAGS=$(CFLAGS) >> .make-settings
	echo LDFLAGS=$(LDFLAGS) >> .make-settings
	echo EXT_CFLAGS=$(EXT_CFLAGS) >> .make-settings
	echo EXT_LDFLAGS=$(EXT_LDFLAGS) >> .make-settings
	echo PREV_FINAL_CFLAGS=$(FINAL_CFLAGS) >> .make-settings
	echo PREV_FINAL_LDFLAGS=$(FINAL_LDFLAGS) >> .make-settings
	cd $(BUILD_ROOT)/deps && $(MAKE) $(DEPENDENCY_TARGETS)

.make-prerequisites:
	@touch $@

ifneq ($(strip $(PREV_FINAL_CFLAGS)), $(strip $(FINAL_CFLAGS)))
.make-prerequisites: persist-settings
endif

ifneq ($(strip $(PREV_FINAL_LDFLAGS)), $(strip $(FINAL_LDFLAGS)))
.make-prerequisites: persist-settings
endif

$(XSOCKS_SERVER_NAME): $(XSOCKS_SERVER_OBJ)
	$(COMMON_LD) -o $@ $^ $(FINAL_LIBS)
	ln -fs builds/src/$@ $(ROOT)/$@

$(XSOCKS_CLIENT_NAME): $(XSOCKS_CLIENT_OBJ)
	$(COMMON_LD) -o $@ $^ $(FINAL_LIBS)
	ln -sf builds/src/$@ $(ROOT)/$@

%.o: $(ROOT)/src/%.c .make-prerequisites
	$(COMMON_CC) -c $<

clean:
	rm -rf $(XSOCKS_SERVER_NAME) $(XSOCKS_CLIENT_NAME) *.o
	rm -rf $(ROOT)/$(XSOCKS_SERVER_NAME) $(ROOT)/$(XSOCKS_CLIENT_NAME)

distclean: clean
	cd $(BUILD_ROOT)/deps && $(MAKE) distclean
	rm -rf .make-*

.PHONY: all clean distclean persist-settings
