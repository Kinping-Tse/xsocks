
BUILD_ROOT = $(realpath ..)
ROOT = $(BUILD_ROOT)/..

include $(BUILD_ROOT)/common.mk

default:
	@echo "Explicit target required"

libev:
	$(QUIET_MAKE)
	cd $@ && $(ROOT)/deps/libev/configure > /dev/null
	cd $@ && $(MAKE) $(MFLAGS) &> /dev/null

redis:
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS) static

json-parser:
	$(QUIET_MAKE)
	cd $@ && $(ROOT)/deps/json-parser/configure > /dev/null
	cd $@ && $(MAKE) $(MFLAGS) libjsonparser.a &> /dev/null

shadowsocks-libev:
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS) static

libbloom:
	$(QUIET_MAKE)
	cd $(ROOT)/deps/$@ && $(MAKE) $(MFLAGS)

clean:
	-(cd libev && $(MAKE) distclean)
	cd redis && $(MAKE) clean
	-(cd json-parser && $(MAKE) distclean)
	cd shadowsocks-libev && $(MAKE) clean
	cd $(ROOT)/deps/libbloom && $(MAKE) clean

distclean: clean
	rm -rf .make-*

.PHONY: default clean distclean libev redis json-parser shadowsocks-libev libbloom
