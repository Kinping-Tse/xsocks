
BUILD_ROOT = ../..
ROOT = $(BUILD_ROOT)/..
VPATH = $(ROOT)/deps/shadowsocks-libev

include $(BUILD_ROOT)/common.mk

WARN = -Wall -Wextra #-Wpedantic

SRCS = $(wildcard $(VPATH)/*.c)
OBJS = $(notdir $(SRCS:.c=.o))

LIBNAME = libshadowsocks-libev

EXT_CFLAGS = -fPIC $(ARCH) -I$(ROOT)/deps/libbloom
EXT_LDFLAGS = $(ARCH)

STLIBNAME = $(LIBNAME).$(STLIB_SUFFIX)
STLIB_MAKE_CMD = ar rcs $(STLIBNAME)

$(DYLIBNAME): $(OBJS)
	$(DYLIB_MAKE_CMD) $(OBJS)

$(STLIBNAME): $(OBJS)
	$(STLIB_MAKE_CMD) $(OBJS)

dynamic: $(DYLIBNAME)

static: $(STLIBNAME)

%.o: %.c
	$(COMMON_CC) -c $<

clean:
	rm -rf $(DYLIBNAME) $(STLIBNAME) *.o

.PHONY: clean
