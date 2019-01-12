
BUILD_ROOT = $(realpath ..)
ROOT = $(BUILD_ROOT)/..

include $(BUILD_ROOT)/common.mk

default:
	@echo "Explicit target required"

libev:
	$(QUIET_MAKE)
	cd $@ && $(ROOT)/deps/libev/configure
	cd $@ && $(MAKE)

redis:
	$(QUIET_MAKE)
	cd $@ && $(MAKE) static

json-parser:
	$(QUIET_MAKE)
	cd $@ && $(ROOT)/deps/json-parser/configure
	cd $@ && $(MAKE) libjsonparser.a

clean:
	-(cd libev && $(MAKE) distclean)
	cd redis && $(MAKE) clean
	-(cd json-parser && $(MAKE) distclean)

distclean: clean
	rm -rf .make-*

.PHONY: default libev redis json-parser clean distclean
