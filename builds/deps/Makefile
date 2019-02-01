
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
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(PREFIX)
	cd $@ && $(MAKE) $(MFLAGS)

JEMALLOC_VERSION := 5.1.0-0-g0

jemalloc:
	$(QUIET_MAKE)
	cd $(DEPS_PATH)/$@ && autoconf -f
	cd $@ && $(DEPS_PATH)/$@/configure --enable-autogen --prefix=$(PREFIX) --with-version=$(JEMALLOC_VERSION) --with-jemalloc-prefix=je_
	cd $@ && $(MAKE) $(MFLAGS)

jemalloc-clean:
	-(cd jemalloc && $(MAKE) relclean)
	cd $(DEPS_PATH)/jemalloc && rm -rf configure

redis: $(REDIS_DEPS)
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS)

JSONPARSER_LDFLAGS = $(LDFLAGS)
ifeq ($(uname_S), Darwin)
JSONPARSER_LDFLAGS += -Wl,-install_name,$(INSTALL_LIB)/$$(REAL_NAME) -compatibility_version 1.1 -current_version 1.1.0
endif

json-parser:
	$(QUIET_MAKE)
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(PREFIX)
	cd $@ && $(MAKE) $(MFLAGS) LDFLAGS='$(JSONPARSER_LDFLAGS)'

shadowsocks-libev: libsodium mbedtls libbloom
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS)

LIBBLOOM_LDFLAGS = $(LDFLAGS)
ifeq ($(uname_S), Darwin)
LIBBLOOM_LDFLAGS += -Wl,-install_name,$(INSTALL_LIB)/libbloom.dylib
endif

libbloom:
	$(QUIET_MAKE)
	cd $@ && $(MAKE) -C $(DEPS_PATH)/$@ $(MFLAGS) BUILD=$(LIBBLOOM_PATH) LDFLAGS="$(LIBBLOOM_LDFLAGS)"

MBEDTLS_LDFLAGS = $(LDFLAGS)
ifeq ($(uname_S), Darwin)
MBEDTLS_LDFLAGS += -Wl,-install_name,$(INSTALL_LIB)/$$@
endif

mbedtls:
	$(QUIET_MAKE)
	cd $@ && $(MAKE) -C $(DEPS_PATH)/$@ $(MFLAGS) lib LDFLAGS='$(MBEDTLS_LDFLAGS)' SHARED=1

libsodium:
	$(QUIET_MAKE)
	cd $(DEPS_PATH)/$@ && sh autogen.sh
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(PREFIX)
	cd $@ && $(MAKE) $(MFLAGS)

libsodium-clean:
	-(cd libsodium && $(MAKE) maintainer-clean)
	cd $(DEPS_PATH)/libsodium && rm -rf $(LIBSODIUM_CONFIG_FILE)
	find $(DEPS_PATH)/libsodium -type f -name "Makefile.in" -exec rm -f {} \;

clean: jemalloc-clean libsodium-clean
	-($(MAKE) -C libev maintainer-clean)
	$(MAKE) -C redis clean
	-($(MAKE) -C json-parser distclean)
	$(MAKE) -C shadowsocks-libev clean
	$(MAKE) -C $(DEPS_PATH)/libbloom clean BUILD=$(LIBBLOOM_PATH)
	$(MAKE) -C $(DEPS_PATH)/mbedtls clean

distclean: clean
	-(find jemalloc/* libsodium/* -type d -exec rm -rf {} \; &> /dev/null)
	rm -rf .make-*

install: install-shared
install-shared:
	-($(MAKE) -C jemalloc install_lib_shared)
	-($(MAKE) -C libev install-libLTLIBRARIES)
	$(MAKE) -C json-parser install-shared
	$(MAKE) -C redis install-shared
	$(MAKE) -C shadowsocks-libev install-shared
	$(MAKE) -C libsodium install-exec
	$(MAKE) -C $(DEPS_PATH)/mbedtls install DESTDIR=$(PREFIX) PREFIX=mbedtls_
	$(COMMON_INSTALL) $(LIBBLOOM_PATH)/libbloom.$(DYLIB_SUFFIX) $(INSTALL_LIB)
	rm -rf $(INSTALL_LIB)/*.a $(INSTALL_LIB)/*.la $(INSTALL_INC) $(INSTALL_DATA)
	rm -rf $(INSTALL_BIN)/mbedtls_* $(INSTALL_LIB)/libmbedx509* $(INSTALL_LIB)/libmbedtls*

.PHONY: default all clean distclean install install-shared libev jemalloc jemalloc-clean redis json-parser shadowsocks-libev libbloom libsodium libsodium-clean mbedtls mbedtls-clean
