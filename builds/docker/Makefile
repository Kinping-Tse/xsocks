
ROOT = $(realpath ../..)

TMP_DOCKER_PATH = .docker

default: docker

docker: clean
	cd $(ROOT) && git archive HEAD . --format tar -o $(ROOT)/builds/docker/$(TMP_DOCKER_PATH).tar
	mkdir -p $(TMP_DOCKER_PATH)
	tar -C $(TMP_DOCKER_PATH) -xf $(TMP_DOCKER_PATH).tar
	cd $(TMP_DOCKER_PATH) && docker build -f docker/Dockerfile . -t xsocks:alpine

clean:
	rm -rf $(TMP_DOCKER_PATH) $(TMP_DOCKER_PATH).tar

.PHONY: default clean docker
