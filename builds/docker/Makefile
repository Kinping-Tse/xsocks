
ROOT = $(realpath ../..)

VERSION = $(shell grep XS_VERSION $(ROOT)/src/core/version.h |awk -F \" '{print $$2}')
default: docker

docker:
	cd $(ROOT) && docker build --force-rm --pull --no-cache -f docker/Dockerfile -t xsocks:$(VERSION)-alpine .

.PHONY: default docker
