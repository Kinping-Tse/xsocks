
ROOT = $(realpath ../..)

TMP_DOCKER_PATH = .docker

VERSION = $(shell grep XS_VERSION $(ROOT)/src/core/version.h |awk -F \" '{print $$2}')
default: docker

docker: clean
	cd $(ROOT) && git archive HEAD . --format tar -o $(ROOT)/builds/docker/$(TMP_DOCKER_PATH).tar
	mkdir -p $(TMP_DOCKER_PATH)
	tar -C $(TMP_DOCKER_PATH) -xf $(TMP_DOCKER_PATH).tar
	# For optimize docker size
	cd $(TMP_DOCKER_PATH)/deps/json-parser && rm -rf bindings examples tests
	cd $(TMP_DOCKER_PATH) && docker build -f docker/Dockerfile -t xsocks:$(VERSION)-alpine .

clean:
	rm -rf $(TMP_DOCKER_PATH) $(TMP_DOCKER_PATH).tar

.PHONY: default clean docker
