
BUILD_ROOT = ..
ROOT = $(BUILD_ROOT)/..
DEPS_PATH = $(ROOT)/deps
BUILD_DEPS_PATH = $(BUILD_ROOT)/deps
VPATH = $(ROOT)/src:$(ROOT)/src/event

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

EVENT = ae
ifeq ($(USE_AE), yes)
	EVENT = ae
endif

ifeq ($(USE_LIBEV), yes)
	EVENT = libev
endif

-include .make-settings

EXT_CFLAGS = -I$(DEPS_PATH)/libev -I$(DEPS_PATH)/redis -I$(DEPS_PATH)/json-parser -I$(DEPS_PATH)/shadowsocks-libev
EXT_LDFLAGS =
EXT_LIBS = $(BUILD_DEPS_PATH)/libev/.libs/libev.a \
	$(BUILD_DEPS_PATH)/redis/libredis.a \
	$(BUILD_DEPS_PATH)/json-parser/libjsonparser.a

ifeq ($(EVENT), ae)
	EXT_CFLAGS += -DUSE_AE
endif

ifeq ($(EVENT), libev)
	EXT_CFLAGS += -DUSE_LIBEV
endif

XSOCKS_SERVER_NAME = xs-server
XSOCKS_SERVER_OBJ = server.o
XSOCKS_LOCAL_NAME = xs-local
XSOCKS_LOCAL_OBJ = local.o utils.o config.o logger.o event.o

all: $(XSOCKS_SERVER_NAME) $(XSOCKS_LOCAL_NAME)

-include Makefile.dep

Makefile.dep:
	$(COMMON_CC) -MM $(ROOT)/src/*.c $(ROOT)/src/event/*.c > Makefile.dep

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

$(XSOCKS_LOCAL_NAME): $(XSOCKS_LOCAL_OBJ)
	$(COMMON_LD) -o $@ $^ $(FINAL_LIBS)
	ln -fs builds/src/$@ $(ROOT)/$@

%.o: %.c .make-prerequisites
	$(COMMON_CC) -c $<

clean:
	rm -rf $(XSOCKS_SERVER_NAME) $(XSOCKS_LOCAL_NAME) *.o Makefile.dep
	rm -rf $(ROOT)/$(XSOCKS_SERVER_NAME) $(ROOT)/$(XSOCKS_LOCAL_NAME)

distclean: clean
	cd $(BUILD_ROOT)/deps && $(MAKE) distclean
	rm -rf .make-*

.PHONY: all clean distclean persist-settings
