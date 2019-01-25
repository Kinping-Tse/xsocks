
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

DEPS_PATH = $(ROOT)/deps
BUILD_ROOT = $(ROOT)/builds
BUILD_DEPS_PATH = $(BUILD_ROOT)/deps

JEMALLOC_PATH = $(BUILD_DEPS_PATH)/jemalloc
LIBEV_PATH = $(BUILD_DEPS_PATH)/libev/.libs
REDIS_PATH = $(BUILD_DEPS_PATH)/redis
SHADOWSOCKS_LIBEV_PATH = $(BUILD_DEPS_PATH)/shadowsocks-libev
JSONPARSER_PATH = $(BUILD_DEPS_PATH)/json-parser/.json-parser
MBEDTLS_PATH = $(BUILD_DEPS_PATH)/mbedtls
LIBBLOOM_PATH = $(BUILD_DEPS_PATH)/libbloom/build
LIBSODIUM_PATH = $(BUILD_DEPS_PATH)/libsodium/.libsodium
