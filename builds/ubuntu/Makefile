
ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

default:
	docker run -it --rm -v $(ROOT):/tmp/xsocks -w /tmp/xsocks xsocks/builds/asuswrt-merlin.ng:$(VERSION) bash

.PHONY: default
