uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
OPTIMIZATION ?= -O2

STD = -std=c99
WARN = -Wall -Wextra -Werror -Wpedantic
OPT = $(OPTIMIZATION)
DEBUG = -g -ggdb

DYLIB_SUFFIX = so
STLIB_SUFFIX = a

ifeq ($(uname_S), Darwin)
DYLIB_SUFFIX = dylib
endif

FINAL_CFLAGS = $(STD) $(WARN) $(OPT) $(DEBUG) $(CFLAGS) $(EXT_CFLAGS)
FINAL_LDFLAGS = $(DEBUG) $(LDFLAGS) $(EXT_LDFLAGS)
FINAL_LIBS = -lm $(EXT_LIBS)

COMMON_CC = $(QUIET_CC) $(CC) $(FINAL_CFLAGS)
COMMON_LD = $(QUIET_LINK) $(CC) $(FINAL_LDFLAGS)
COMMON_INSTALL = $(QUIET_INSTALL) $(INSTALL)

CCCOLOR = "\033[34m"
LINKCOLOR = "\033[34;1m"
SRCCOLOR = "\033[33m"
BINCOLOR = "\033[37;1m"
MAKECOLOR = "\033[32;1m"
ENDCOLOR = "\033[0m"

ifndef V
QUIET_CC = @printf '    %b %b\n' $(CCCOLOR)CC$(ENDCOLOR) $(SRCCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_LINK = @printf '    %b %b\n' $(LINKCOLOR)LINK$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_MAKE = @printf '    %b %b\n' $(MAKECOLOR)MAKE$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
QUIET_INSTALL = @printf '    %b %b\n' $(LINKCOLOR)INSTALL$(ENDCOLOR) $(BINCOLOR)$@$(ENDCOLOR) 1>&2;
endif
