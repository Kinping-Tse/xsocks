
BUILD_ROOT = $(realpath ..)
ROOT = $(BUILD_ROOT)/..

include $(BUILD_ROOT)/common.mk

LIBSODIUM_CONFIG_FILE = m4/argz.m4 m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4 configure aclocal.m4 autom4te.cache build-aux

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

shadowsocks-libev: libsodium mbedtls
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS) static

libbloom:
	$(QUIET_MAKE)
	cd $(ROOT)/deps/$@ && $(MAKE) $(MFLAGS)

mbedtls:
	$(QUIET_MAKE)
	cd $(ROOT)/deps/$@ && $(MAKE) $(MFLAGS) lib
	cd $(ROOT)/deps/$@ && $(MAKE) install DESTDIR=$(BUILD_ROOT)/deps/mbedtls

mbedtls-clean:
	cd $(ROOT)/deps/mbedtls && $(MAKE) clean
	cd $(ROOT)/deps/mbedtls && $(MAKE) uninstall DESTDIR=$(BUILD_ROOT)/deps/mbedtls

libsodium:
	$(QUIET_MAKE)
	cd $(ROOT)/deps/libsodium && sh autogen.sh &> /dev/null
	cd $@ && $(ROOT)/deps/libsodium/configure --prefix=$(BUILD_ROOT)/deps/libsodium/.libsodium > /dev/null
	cd $@ && $(MAKE) $(MFLAGS) && $(MAKE) install

libsodium-clean:
	-(cd libsodium && $(MAKE) distclean && rm -rf .libsodium)
	cd $(ROOT)/deps/libsodium && rm -rf $(LIBSODIUM_CONFIG_FILE)
	find $(ROOT)/deps/libsodium -type f -name "Makefile.in" -exec rm -f {} \;

clean: mbedtls-clean libsodium-clean
	-(cd libev && $(MAKE) distclean)
	cd redis && $(MAKE) clean
	-(cd json-parser && $(MAKE) distclean)
	cd shadowsocks-libev && $(MAKE) clean
	cd $(ROOT)/deps/libbloom && $(MAKE) clean

distclean: clean
	rm -rf .make-*

.PHONY: default clean distclean libev redis json-parser shadowsocks-libev \
	libbloom libsodium libsodium-clean mbedtls mbedtls-clean
