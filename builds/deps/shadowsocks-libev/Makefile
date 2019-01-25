
BUILD_ROOT = ../..
ROOT = $(BUILD_ROOT)/..
BUILD_DEPS_PATH = $(BUILD_ROOT)/deps
VPATH = $(ROOT)/deps/shadowsocks-libev

include $(BUILD_ROOT)/common.mk

SRCS = $(wildcard $(VPATH)/*.c)
OBJS = $(notdir $(SRCS:.c=.o))
LIBNAME = libshadowsocks-libev
DYLIBNAME = $(LIBNAME).$(DYLIB_SUFFIX)
STLIBNAME = $(LIBNAME).$(STLIB_SUFFIX)

LIBBLOOM_PATH = $(BUILD_DEPS_PATH)/libbloom/build
MBEDTLS_PATH = $(BUILD_DEPS_PATH)/mbedtls
LIBSODIUM_PATH = $(BUILD_DEPS_PATH)/libsodium/.libsodium

WARN = -Wall -Wextra #-Wpedantic
EXT_CFLAGS = -fPIC -I$(ROOT)/deps/libbloom -I$(MBEDTLS_PATH)/include -I$(LIBSODIUM_PATH)/include
EXT_LDFLAGS = -shared -L$(LIBBLOOM_PATH) -L$(MBEDTLS_PATH)/lib -L$(LIBSODIUM_PATH)/lib
EXT_LIBS += -lbloom -lmbedcrypto -lsodium
ARFLAGS =
EXT_ARFLAGS = -rcs

all: dynamic static
dynamic: $(DYLIBNAME)
static: $(STLIBNAME)

$(DYLIBNAME): $(OBJS)
	$(COMMON_LD) -o $@ $^ $(FINAL_LIBS)

$(STLIBNAME): $(OBJS)
	$(COMMON_AR) $@ $^

%.o: %.c
	$(COMMON_CC) -c $<

clean:
	rm -rf $(DYLIBNAME) $(STLIBNAME) *.o

.PHONY: all dynamic static clean
