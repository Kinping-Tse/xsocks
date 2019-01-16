
default: all

.DEFAULT:
	cd builds/src && $(MAKE) $(MKFLAGS) $@
