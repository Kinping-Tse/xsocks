
default: all

.DEFAULT:
	$(MAKE) -C builds/src $(MKFLAGS) $@

install:
	$(MAKE) -C builds/src $@

docker:
	$(MAKE) -C builds/docker $@

asuswrt-merlin.ng:
	$(MAKE) -C builds/asuswrt-merlin.ng

.PHONY: install docker asuswrt-merlin.ng
