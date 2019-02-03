
ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

SRC_ROOT = $(ROOT)/src
VPATH = $(SRC_ROOT)/core:$(SRC_ROOT)/event:$(SRC_ROOT)/module

include $(BUILD_ROOT)/common.mk

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

EXT_CFLAGS = -I$(DEPS_PATH) $(LIBSODIUM_HEADER_CFLAGS) -I$(MBEDTLS_PATH)/include
EXT_LDFLAGS =
EXT_LIBS = -lm

ifeq ($(LINK_MODE), static)
EXT_LIBS += $(REDIS_PATH)/libredis.a \
	$(SHADOWSOCKS_LIBEV_PATH)/libshadowsocks-libev.a \
	$(JSONPARSER_PATH)/libjsonparser.a \
	$(LIBBLOOM_PATH)/libbloom.a \
	$(MBEDTLS_PATH)/library/libmbedcrypto.a \
	$(LIBSODIUM_PATH)/.libs/libsodium.a
	ifeq ($(MALLOC), jemalloc)
		EXT_LIBS += $(JEMALLOC_PATH)/lib/libjemalloc.a
		ifeq ($(uname_S), Linux)
		EXT_LIBS += -pthread -ldl -lrt
		endif
	endif
endif
ifeq ($(LINK_MODE), shared)
EXT_LDFLAGS += -L$(REDIS_PATH) \
	-L$(SHADOWSOCKS_LIBEV_PATH) \
	-L$(JSONPARSER_PATH)
	ifeq ($(uname_S), Linux)
	EXT_LDFLAGS += -Wl,-rpath,$(INSTALL_LIB),-rpath,$(JEMALLOC_PATH)/lib,-rpath,$(LIBBLOOM_PATH),-rpath,$(MBEDTLS_PATH)/library,-rpath,$(LIBSODIUM_PATH)/.libs
	endif
EXT_LIBS += -lredis -ljsonparser -lshadowsocks-libev
endif

ifeq ($(EVENT), ae)
	EXT_CFLAGS += -DUSE_AE
endif
ifeq ($(EVENT), libev)
	EXT_CFLAGS += -DUSE_LIBEV -Wno-strict-aliasing
	DEPENDENCY_TARGETS += libev
	ifeq ($(LINK_MODE), static)
		EXT_LIBS += $(LIBEV_PATH)/.libs/libev.a
	endif
	ifeq ($(LINK_MODE), shared)
		EXT_LDFLAGS += -L$(LIBEV_PATH)/.libs
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
	$(COMMON_CC) -MM $(SRC_ROOT)/*/*.c > Makefile.dep 2> /dev/null

persist-settings: distclean
	@printf "Set env to to %b\n" $(COLOR_INFO)$(BUILD_ROOT)/src/.make-settings$(ENDCOLOR)
	@echo STD=$(STD) >> .make-settings
	@echo WARN=$(WARN) >> .make-settings
	@echo OPT=$(OPT) >> .make-settings
	@echo MALLOC=$(MALLOC) >> .make-settings
	@echo EVENT=$(EVENT) >> .make-settings
	@echo CFLAGS=$(CFLAGS) >> .make-settings
	@echo LDFLAGS=$(LDFLAGS) >> .make-settings
	@echo EXT_CFLAGS=$(EXT_CFLAGS) >> .make-settings
	@echo EXT_LDFLAGS=$(EXT_LDFLAGS) >> .make-settings
	@echo PREV_FINAL_CFLAGS=$(FINAL_CFLAGS) >> .make-settings
	@echo PREV_FINAL_LDFLAGS=$(FINAL_LDFLAGS) >> .make-settings
	@echo LINK_MODE=$(LINK_MODE) >> .make-settings
	@echo PREFIX=$(PREFIX) >> .make-settings
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

%.o: %.c .make-prerequisites
	$(COMMON_CC) -c $<

install: all install-lib
	@mkdir -p $(INSTALL_BIN) $(INSTALL_ETC)
	$(COMMON_INSTALL) $(XSOCKS_MODULE_EXE) $(INSTALL_BIN)
	$(COMMON_INSTALL) $(ROOT)/config.json $(INSTALL_ETC)
	@printf "\nInstall to %b\n\n" $(COLOR_INFO)$(PREFIX)$(ENDCOLOR)

install-lib:

ifeq ($(LINK_MODE), shared)
install-lib:
	@mkdir -p $(INSTALL_LIB)
	$(MAKE) -C $(BUILD_DEPS_PATH) install-shared
endif

clean:
	rm -rf $(XSOCKS_MODULE_EXE) *.o Makefile.dep

distclean: clean
	$(MAKE) -C $(BUILD_DEPS_PATH) distclean
	rm -rf .make-*

.PHONY: all install install-lib clean distclean persist-settings
