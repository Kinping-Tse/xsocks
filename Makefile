
default: all

.DEFAULT:
	$(MAKE) -C builds/src $(MKFLAGS) $@

install:
	$(MAKE) -C builds/src $@

.PHONY: install
