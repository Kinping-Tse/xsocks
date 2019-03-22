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

default: all

.DEFAULT:
	$(MAKE) -C builds/src $(MKFLAGS) $@

install:
	$(MAKE) -C builds/src $@

docker:
	$(MAKE) -C builds/$@

asuswrt-merlin.ng:
	$(MAKE) -C builds/$@

ubuntu:
	$(MAKE) -C builds/$@

.PHONY: install docker asuswrt-merlin.ng ubuntu
