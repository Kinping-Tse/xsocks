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

ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

HOST ?= aarch64-linux
AM_TOOLCHAINS_PATH ?= /usr/local/src/github.com/RMerl/am-toolchains

IMAGE = alucard5867/xsocks/builds/asuswrt-merlin.ng:$(VERSION)

default: docker

docker: image
	@printf "\n%b: Please check %b correct: %b\n\n" $(COLOR_WARN)WARNING$(ENDCOLOR) $(COLOR_WARN)AM_TOOLCHAINS_PATH$(ENDCOLOR) $(COLOR_INFO)$(AM_TOOLCHAINS_PATH)$(ENDCOLOR)

	# We don't use docker-compose yet, it's so slow!
	# ROOT=$(ROOT) HOST=$(HOST) AM_TOOLCHAINS_PATH=$(AM_TOOLCHAINS_PATH) docker-compose up --build
	docker run -it --rm \
		-e ROOT=/tmp/xsocks -e HOST=$(HOST) \
		-v $(ROOT):/tmp/xsocks -v $(AM_TOOLCHAINS_PATH):/tmp/am-toolchains \
		-w /tmp/xsocks/builds/asuswrt-merlin.ng $(IMAGE) bash

image:
	docker build --pull --force-rm -t $(IMAGE) .

.PHONY: default docker image
