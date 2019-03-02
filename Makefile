
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
