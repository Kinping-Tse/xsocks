
default: all

.DEFAULT:
	$(MAKE) -C builds/src $(MKFLAGS) $@

install:
	$(MAKE) -C builds/src $@

docker:
	$(MAKE) -C builds/docker $@

.PHONY: install docker
