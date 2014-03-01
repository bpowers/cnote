include config.mk

NAME = cnote
VERSION = $(shell grep VERSION config.h | cut -d '"' -f 2)

HOST ?= lightswitchrave.net

EXE = src/$(NAME)
PKGNAME = $(NAME)
RH_VERSION = fc$(shell head -n1 /etc/issue | cut -d ' ' -f 3)
RPMSHORT = $(PKGNAME)-$(VERSION)-1.$(RH_VERSION).x86_64.rpm
RPM = package/RPMS/x86_64/$(RPMSHORT)

# if you invoke make as 'make V=1' it will verbosely list what it is doing,
# otherwise it defaults to pretty mode, which makes errors _much_ easier to see
ifneq ($V, 1)
  MAKEFLAGS = -s
endif


all: build

build: vendor/jemalloc/Makefile
	$(MAKE) -C vendor/jemalloc
	$(MAKE) -C src V=$(V)

distclean: clean
	rm vendor/jemalloc/Makefile
	$(MAKE) -C src distclean V=$(V)

clean:
	rm -rf package site
	$(MAKE) -C vendor/jemalloc clean
	$(MAKE) -C src clean V=$(V)

check:
	$(MAKE) -C src check V=$(V)

install:
	$(MAKE) -C src install V=$(V)

coverage:
	$(MAKE) -C src coverage V=$(V)

put: $(RPM)
	rsync -az $(RPM) $(HOST):.
	ssh $(HOST) -t sudo rpm --force -fvi ./$(RPMSHORT)

rpm: $(RPM)

$(RPM): build
	mkdir -p site
	cp -a $(EXE) site/
	cp -a site $(PKGNAME)-$(VERSION)
	mkdir -p package/{RPMS,BUILD,SOURCES,BUILDROOT}
	tar -czf package/SOURCES/$(PKGNAME)-$(VERSION).tar.gz $(PKGNAME)-$(VERSION)
	rm -rf $(PKGNAME)-$(VERSION)
	cat server.service.in | sed "s/%NAME%/$(NAME)/g" >package/SOURCES/server.service
	cat server.spec.in | sed "s/%NAME%/$(NAME)/g" | sed "s/%VERSION%/$(VERSION)/g" >server.spec
	rpmbuild --define "_topdir $(PWD)/package" -ba server.spec
	rm -rf package/{BUILD,BUILDROOT}


vendor/jemalloc/Makefile:
	git submodule update --init
	cd vendor/jemalloc && ./autogen.sh

.PHONY: all rpm build distclean clean check coverage
