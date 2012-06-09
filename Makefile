include config.mk

# if you invoke make as 'make V=1' it will verbosely list what it is doing,
# otherwise it defaults to pretty mode, which makes errors _much_ easier to see
ifneq ($V, 1)
MAKEFLAGS = -s
endif

build: vendor/jemalloc/Makefile
	$(MAKE) -C vendor/jemalloc
	$(MAKE) -C src V=$(V)

distclean: clean
	rm vendor/jemalloc/Makefile
	$(MAKE) -C src distclean V=$(V)

clean:
	$(MAKE) -C vendor/jemalloc clean
	$(MAKE) -C src clean V=$(V)

check:
	$(MAKE) -C src check V=$(V)

install:
	$(MAKE) -C src install V=$(V)

coverage:
	$(MAKE) -C src coverage V=$(V)

vendor/jemalloc:
	git submodule update --init

vendor/jemalloc/Makefile:
	cd vendor/jemalloc && ./autogen.sh CC=clang CFLAGS="-O4" LDFLAGS="-flto"

.PHONY: build distclean clean check coverage
