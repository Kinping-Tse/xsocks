
ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

HOST ?= aarch64-linux
AM_TOOLCHAINS_PATH ?= /usr/local/src/github.com/RMerl/am-toolchains

default: docker

docker:
	@printf "\n%b: Please check %b correct: %b\n\n" $(COLOR_WARN)WARNING$(ENDCOLOR) $(COLOR_WARN)AM_TOOLCHAINS_PATH$(ENDCOLOR) $(COLOR_INFO)$(AM_TOOLCHAINS_PATH)$(ENDCOLOR)
	ROOT=$(ROOT) XSOCKS_VERSION=$(VERSION) HOST=$(HOST) AM_TOOLCHAINS_PATH=$(AM_TOOLCHAINS_PATH) docker-compose up --build

.PHONY: default docker
