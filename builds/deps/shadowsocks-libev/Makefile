
ROOT = $(realpath ../../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

VPATH = $(DEPS_PATH)/shadowsocks-libev

SRCS = $(wildcard $(VPATH)/*.c)
OBJS = $(notdir $(SRCS:.c=.o))
LIBNAME = libshadowsocks-libev
DYLIBNAME = $(LIBNAME).$(DYLIB_SUFFIX)
STLIBNAME = $(LIBNAME).$(STLIB_SUFFIX)

WARN = -Wall -Wextra #-Wpedantic
EXT_CFLAGS = -fPIC -I$(ROOT)/deps/libbloom -I$(MBEDTLS_PATH)/include -I$(LIBSODIUM_PATH)/include
EXT_LDFLAGS = -shared -L$(LIBBLOOM_PATH) -L$(MBEDTLS_PATH)/lib -L$(LIBSODIUM_PATH)/lib -Wl,-rpath,$(PREFIX)/lib,-rpath,$(LIBBLOOM_PATH),-rpath,$(MBEDTLS_PATH)/lib,-rpath,$(LIBSODIUM_PATH)/lib
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
