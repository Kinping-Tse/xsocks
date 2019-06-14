#
# This file is part of xsocks, a lightweight proxy tool for science online.
#
# Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

ROOT = $(realpath ../../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

LIBIPSET_SRC = $(LIBIPSET_SRC_PATH)/src
VPATH = $(LIBIPSET_SRC) $(wildcard $(LIBIPSET_SRC)/*)

SRCS = $(wildcard $(LIBIPSET_SRC)/*.c) $(wildcard $(LIBIPSET_SRC)/*/*.c)
OBJS = general.o \
	assignments.o basics.o bdd-iterator.o expanded.o reachable.o read.o write.o \
	map_allocation.o map_inspection.o ipv4_map.o ipv6_map.o map_storage.o \
	set_allocation.o set_inspection.o ipv4_set.o ipv6_set.o iterator.o set_storage.o

LIBNAME = libipset

include $(ROOT)/builds/lib.mk

WARN = -Wall -Wextra -Wno-unused-parameter
EXT_CFLAGS += $(LIBIPSET_HEADER_CFLAGS) $(LIBCORK_HEADER_CFLAGS)
EXT_LDFLAGS += $(LIBCORK_LIB_LDFLAGS)
EXT_LIBS += -lcork
BUILD_TMP_FILES += map

CC_CUSTOM = $(COMMON_CC) -c $< -o $@
map_allocation.o: map/allocation.c
	$(CC_CUSTOM)
map_inspection.o: map/inspection.c
	$(CC_CUSTOM)
map_storage.o: map/storage.c
	$(CC_CUSTOM)
set_allocation.o: set/allocation.c
	$(CC_CUSTOM)
set_inspection.o: set/inspection.c
	$(CC_CUSTOM)
set_storage.o: set/storage.c
	$(CC_CUSTOM)
