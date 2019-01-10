
ROOT = $(realpath ../..)

JSONPARSER_ROOT = $(ROOT)/deps/json-parser

default:
	@echo "Explicit target required"

libev:
	$(QUIET_MAKE)
	cd libev && $(ROOT)/deps/libev/configure
	cd libev && $(MAKE)

redis:
	$(QUIET_MAKE)
	cd redis && $(MAKE) static

json-parser:
	$(QUIET_MAKE)
	cd $(JSONPARSER_ROOT) && ./configure
	cd $(JSONPARSER_ROOT) && $(MAKE) libjsonparser.a

clean:
	-(cd libev && $(MAKE) distclean)
	cd redis && $(MAKE) clean
	-(cd $(JSONPARSER_ROOT) && $(MAKE) clean)
	cd $(JSONPARSER_ROOT) && rm -rf Makefile config.log config.status json-parser.pc

distclean: clean
	rm -rf .make-*

.PHONY: default libev redis json-parser clean distclean
