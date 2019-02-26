
ROOT = $(realpath ../../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

VPATH = $(DEPS_PATH)/redis

OBJS = anet.o zmalloc.o ae.o sds.o adlist.o
LIBNAME = libredis
DYLIBNAME = $(LIBNAME).$(DYLIB_SUFFIX)
STLIBNAME = $(LIBNAME).$(STLIB_SUFFIX)

EXT_CFLAGS = -fPIC $(ARCH)
EXT_LDFLAGS = -shared $(ARCH)
ARFLAGS =
EXT_ARFLAGS = -rcs

ifeq ($(MALLOC), jemalloc)
	EXT_CFLAGS += -DUSE_JEMALLOC -I$(JEMALLOC_PATH)/include
	EXT_LDFLAGS += -L$(JEMALLOC_PATH)/lib
	EXT_LIBS += -ljemalloc
endif

DYLIB_MAKE_CMD = $(COMMON_LD)
ifeq ($(uname_S), Darwin)
DYLIB_MAKE_CMD = $(COMMON_LD) -Wl,-install_name,$(PREFIX)/lib/$(DYLIBNAME)
endif

all: dynamic static
dynamic: $(DYLIBNAME)
static: $(STLIBNAME)

install-shared: $(DYLIBNAME)
	$(COMMON_INSTALL) -d $(INSTALL_LIB)
	$(COMMON_INSTALL) -m 755 $(DYLIBNAME) $(INSTALL_LIB)

$(DYLIBNAME): $(OBJS)
	$(DYLIB_MAKE_CMD) -o $@ $^ $(FINAL_LIBS)

$(STLIBNAME): $(OBJS)
	$(COMMON_AR) $@ $^

%.o: %.c
	$(COMMON_CC) -c $<

clean:
	rm -rf $(DYLIBNAME) $(STLIBNAME) *.o

.PHONY: all install-shared dynamic static clean
