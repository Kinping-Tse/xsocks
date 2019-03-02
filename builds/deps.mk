
VERSION = $(shell grep XS_VERSION $(ROOT)/src/core/version.h | awk -F \" '{print $$2}')

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
LIBSODIUM_PATH = $(BUILD_DEPS_PATH)/libsodium/src/libsodium
LIBSODIUM_SRC_PATH = $(DEPS_PATH)/libsodium

LIBSODIUM_HEADER_CFLAGS = -I$(DEPS_PATH)/libsodium/src/libsodium/include \
	-I$(DEPS_PATH)/libsodium/src/libsodium/include/sodium \
	-I$(LIBSODIUM_PATH)/include
