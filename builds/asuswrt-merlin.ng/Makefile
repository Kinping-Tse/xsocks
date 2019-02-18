
ROOT = $(realpath ../..)

VERSION = $(shell grep XS_VERSION $(ROOT)/src/core/version.h | awk -F \" '{print $$2}')

HOST ?= aarch64-linux
AM_TOOLCHAINS_PATH ?= /usr/local/src/github.com/RMerl/am-toolchains

default: docker

docker:
	ROOT=$(ROOT) XSOCKS_VERSION=$(VERSION) HOST=$(HOST) AM_TOOLCHAINS_PATH=$(AM_TOOLCHAINS_PATH) docker-compose up --build

.PHONY: default docker
