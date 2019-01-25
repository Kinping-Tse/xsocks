
ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

LIBSODIUM_CONFIG_FILE = m4/argz.m4 m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4 configure aclocal.m4 build-aux
REDIS_DEPS =

ifeq ($(MALLOC), jemalloc)
	REDIS_DEPS += jemalloc
endif

# default:
	# @echo "Explicit target required"

default: all

all: redis json-parser shadowsocks-libev

libev:
	$(QUIET_MAKE)
	cd $@ && $(DEPS_PATH)/$@/configure &> $@.configure.log
	cd $@ && $(MAKE) $(MFLAGS) &> $@.make.log

JEMALLOC_VERSION := 5.1.0-0-g0

jemalloc:
	$(QUIET_MAKE)
	cd $(DEPS_PATH)/$@ && autoconf -f
	cd $@ && $(DEPS_PATH)/$@/configure --enable-autogen --with-version=$(JEMALLOC_VERSION) --with-jemalloc-prefix=je_ &> $@.configure.log
	cd $@ && $(MAKE) $(MFLAGS) &> $@.make.log

jemalloc-clean:
	-(cd jemalloc && $(MAKE) relclean)
	cd $(DEPS_PATH)/jemalloc && rm -rf configure

redis: $(REDIS_DEPS)
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS)

json-parser:
	$(QUIET_MAKE)
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(JSONPARSER_PATH) > $@.configure.log
	cd $@ && $(MAKE) $(MFLAGS) install > $@.make.log

shadowsocks-libev: libsodium mbedtls libbloom
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS)

libbloom:
	$(QUIET_MAKE)
	cd $@ && $(MAKE) -C $(DEPS_PATH)/$@ $(MFLAGS) > $@.make.log BUILD=$(LIBBLOOM_PATH)

mbedtls:
	$(QUIET_MAKE)
	cd $@ && $(MAKE) -C $(DEPS_PATH)/$@ $(MFLAGS) lib > $@.make.log SHARED=1
	cd $@ && $(MAKE) -C $(DEPS_PATH)/$@ install DESTDIR=$(MBEDTLS_PATH) >> $@.make.log

mbedtls-clean:
	$(MAKE) -C $(DEPS_PATH)/mbedtls clean
	$(MAKE) -C $(DEPS_PATH)/mbedtls uninstall DESTDIR=$(MBEDTLS_PATH)

libsodium:
	$(QUIET_MAKE)
	cd $(DEPS_PATH)/$@ && sh autogen.sh &> $(BUILD_DEPS_PATH)/$@/$@.autotool.log
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(LIBSODIUM_PATH) > $@.configure.log
	cd $@ && $(MAKE) $(MFLAGS) > $@.make.log && $(MAKE) install >> $@.make.log

libsodium-clean:
	-(cd libsodium && $(MAKE) maintainer-clean && rm -rf $(LIBSODIUM_PATH))
	cd $(DEPS_PATH)/libsodium && rm -rf $(LIBSODIUM_CONFIG_FILE)
	find $(DEPS_PATH)/libsodium -type f -name "Makefile.in" -exec rm -f {} \;

clean: jemalloc-clean mbedtls-clean libsodium-clean
	-(cd libev && $(MAKE) maintainer-clean)
	cd redis && $(MAKE) clean
	-(cd json-parser && $(MAKE) distclean && rm -rf $(JSONPARSER_PATH))
	cd shadowsocks-libev && $(MAKE) clean
	$(MAKE) -C $(DEPS_PATH)/libbloom clean BUILD=$(LIBBLOOM_PATH)

distclean: clean
	rm -rf */*.configure.log */*.make.log */*.autotool.log
	rm -rf .make-*

.PHONY: default all clean distclean libev jemalloc jemalloc-clean redis json-parser shadowsocks-libev libbloom libsodium libsodium-clean mbedtls mbedtls-clean
