
BUILD_ROOT = ../..
ROOT = $(BUILD_ROOT)/..
VPATH = $(ROOT)/deps/redis

include $(BUILD_ROOT)/common.mk

OBJS = anet.o zmalloc.o ae.o sds.o
LIBNAME = libredis
DYLIBNAME = $(LIBNAME).$(DYLIB_SUFFIX)
STLIBNAME = $(LIBNAME).$(STLIB_SUFFIX)

EXT_CFLAGS = -fPIC $(ARCH)
EXT_LDFLAGS = -shared $(ARCH)
ARFLAGS =
EXT_ARFLAGS = -rcs

ifeq ($(MALLOC), jemalloc)
	EXT_CFLAGS += -DUSE_JEMALLOC -I$(BUILD_ROOT)/deps/jemalloc/include
	EXT_LDFLAGS += -L$(BUILD_ROOT)/deps/jemalloc/lib
	EXT_LIBS += -ljemalloc
endif

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
