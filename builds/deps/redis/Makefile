
BUILD_ROOT = ../..
ROOT = $(BUILD_ROOT)/..

include $(BUILD_ROOT)/common.mk

OBJ = anet.o zmalloc.o ae.o
LIBNAME = libredis

EXT_CFLAGS = -fPIC $(ARCH)
EXT_LDFLAGS = $(ARCH)

STLIBNAME = $(LIBNAME).$(STLIB_SUFFIX)
STLIB_MAKE_CMD = ar rcs $(STLIBNAME)

$(DYLIBNAME): $(OBJ)
	$(DYLIB_MAKE_CMD) $(OBJ)

$(STLIBNAME): $(OBJ)
	$(STLIB_MAKE_CMD) $(OBJ)

dynamic: $(DYLIBNAME)
static: $(STLIBNAME)

%.o: $(ROOT)/deps/redis/%.c
	$(COMMON_CC) -c $<

clean:
	rm -rf $(DYLIBNAME) $(STLIBNAME) *.o

.PHONY: clean
