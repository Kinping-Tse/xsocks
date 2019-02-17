
ROOT = $(realpath ../..)
include $(ROOT)/builds/common.mk
include $(ROOT)/builds/deps.mk

REDIS_DEPS =

ifeq ($(MALLOC), jemalloc)
	REDIS_DEPS += jemalloc
endif

GIT_SUBMODULE_DEPS = jemalloc libbloom libsodium mbedtls libev
GIT_SUBMODULE_INIT_FILE = .submodule.lock

default:
	@echo "Explicit target required"

deps-init: | $(GIT_SUBMODULE_INIT_FILE)

$(GIT_SUBMODULE_INIT_FILE):
	@touch $@
	cd $(ROOT) && git submodule update --init --recursive
	cd $(DEPS_PATH)/jemalloc && git checkout -B 5.1.0 refs/tags/5.1.0
	cd $(DEPS_PATH)/libbloom && git checkout -B v1.5 refs/tags/v1.5
	cd $(DEPS_PATH)/libsodium && git checkout -B 1.0.17 refs/tags/1.0.17
	cd $(DEPS_PATH)/mbedtls && git checkout -B mbedtls-2.16.0 refs/tags/mbedtls-2.16.0

libev:
	$(QUIET_MAKE)
	@mkdir -p $@
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(PREFIX)
	cd $@ && $(MAKE) $(MFLAGS)

JEMALLOC_VERSION := 5.1.0-0-g0

jemalloc:
	$(QUIET_MAKE)
	@mkdir -p $@
	cd $(DEPS_PATH)/$@ && autoconf -f
	cd $@ && $(DEPS_PATH)/$@/configure --enable-autogen --prefix=$(PREFIX) --with-version=$(JEMALLOC_VERSION) --with-jemalloc-prefix=je_
	cd $@ && $(MAKE) $(MFLAGS)

redis: $(REDIS_DEPS)
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS)

JSONPARSER_LDFLAGS = $(LDFLAGS)
ifeq ($(uname_S), Darwin)
JSONPARSER_LDFLAGS += -Wl,-install_name,$(INSTALL_LIB)/$$(REAL_NAME) -compatibility_version 1.1 -current_version 1.1.0
endif

json-parser:
	$(QUIET_MAKE)
	@mkdir -p $@
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(PREFIX)
	cd $@ && $(MAKE) $(MFLAGS) LDFLAGS='$(JSONPARSER_LDFLAGS)'

shadowsocks-libev: libsodium mbedtls libbloom
	$(QUIET_MAKE)
	cd $@ && $(MAKE) $(MFLAGS)

LIBBLOOM_LDFLAGS = $(LDFLAGS)
ifeq ($(uname_S), Linux)
LIBBLOOM_LDFLAGS += -Wl,-rpath,$(INSTALL_LIB)
endif
ifeq ($(uname_S), Darwin)
LIBBLOOM_LDFLAGS += -Wl,-install_name,$(INSTALL_LIB)/libbloom.dylib
endif

libbloom:
	$(QUIET_MAKE)
	@mkdir -p $@
	cd $@ && $(MAKE) -C $(DEPS_PATH)/$@ $(MFLAGS) BUILD=$(LIBBLOOM_PATH) LDFLAGS="$(LIBBLOOM_LDFLAGS)" LD_SONAME=""

MBEDTLS_LDFLAGS = $(LDFLAGS)
ifeq ($(uname_S), Darwin)
MBEDTLS_LDFLAGS += -Wl,-install_name,$(INSTALL_LIB)/$$@
endif

mbedtls:
	$(QUIET_MAKE)
	@mkdir -p $@
	cd $@ && $(MAKE) -C $(DEPS_PATH)/$@ $(MFLAGS) lib LDFLAGS='$(MBEDTLS_LDFLAGS)' SHARED=1

libsodium:
	$(QUIET_MAKE)
	@mkdir -p $@
	cd $(DEPS_PATH)/$@ && sh autogen.sh
	cd $@ && $(DEPS_PATH)/$@/configure --prefix=$(PREFIX)
	cd $@ && $(MAKE) $(MFLAGS)

clean:
	-($(MAKE) -C libev maintainer-clean)
	-($(MAKE) -C jemalloc relclean)
	$(MAKE) -C redis clean
	-($(MAKE) -C json-parser distclean)
	$(MAKE) -C shadowsocks-libev clean
	-($(MAKE) -C $(DEPS_PATH)/libbloom clean BUILD=$(LIBBLOOM_PATH))
	-($(MAKE) -C $(DEPS_PATH)/mbedtls clean)
	-($(MAKE) -C libsodium maintainer-clean)

LIBSODIUM_CONFIG_FILE = m4/argz.m4 m4/libtool.m4 m4/ltoptions.m4 m4/ltsugar.m4 m4/ltversion.m4 m4/lt~obsolete.m4 configure aclocal.m4 build-aux autom4te.cache

distclean: clean
	rm -rf $(GIT_SUBMODULE_INIT_FILE) $(GIT_SUBMODULE_DEPS) json-parser
	rm -rf .make-*
	rm -rf $(JEMALLOC_SRC_PATH)/configure
	cd $(LIBSODIUM_SRC_PATH) && rm -rf $(LIBSODIUM_CONFIG_FILE) \
		&& find . -type f -name "Makefile.in" -exec rm -f {} \;

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

.PHONY: default deps-init clean distclean install install-shared redis json-parser shadowsocks-libev $(GIT_SUBMODULE_DEPS)
