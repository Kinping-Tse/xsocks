
ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

default: docker

docker:
	cd $(ROOT) && docker build --force-rm --pull --no-cache -f docker/Dockerfile -t xsocks:$(VERSION)-alpine .

.PHONY: default docker
