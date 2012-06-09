include ../config.mk

# if you invoke make as 'make V=1' it will verbosely list what it is
# doing, otherwise it defaults to pretty mode, which makes errors
# _much_ easier to see
ifneq ($V, 1)
MAKEFLAGS = -s
endif

# for backtrace(3)
#LDFLAGS += -rdynamic

PREFIX=/usr/local

WARNFLAGS := \
        -Wformat -Wall -Wundef -Wpointer-arith -Wcast-qual \
        -Wwrite-strings -Wsign-compare -Wmissing-noreturn \
        -Wextra -Wstrict-aliasing=2 -Wno-unused-parameter

CFLAGS += -I.. -I. -I../vendor
CFLAGS += -Iinclude $(WARNFLAGS) -I.

# get the git HEAD and mark if the index is dirty
HEAD   = $(shell git status | grep --quiet modified; \
	 if [ $$? -eq 0 ]; then echo `git log -n1 --pretty=format:%h`_DIRTY; \
	 else git log -n1 --pretty=format:%h; fi)

# clang doesn't support gcov-based profiling
ifneq ($(CC),clang)
	CFLAGS += -std=gnu99 -Wunsafe-loop-optimizations
	ifneq (,$(findstring -D_PROF, $(CFLAGS)))
		CFLAGS += -ftest-coverage -fprofile-arcs
	endif
else
	CFLAGS += -fcatch-undefined-behavior
endif

# each module will add to this
LIB_SRC := dirwatch.c list.c queries.c tags.c utils.c

SRC := main.c

# determine the object files
LIB_OBJS := $(patsubst %.c,%.o,$(filter %.c,$(LIB_SRC)))
OBJS := $(patsubst %.c,%.o,$(filter %.c,$(SRC)))
OBJS += $(shell find ../vendor/jemalloc/src -name '*.o' | grep -v pic)

BINARY := cnote

TARGETS := $(BINARY)

# clear out all suffixes
.SUFFIXES:
# list only those we use
.SUFFIXES: .d .c .o .test


all: version $(TARGETS) $(TESTS)

version.h: version

.prefix:
	echo "$(PREFIX)" > .new_prefix
	if [ ! -e .prefix ] || [ `diff -u .prefix .new_prefix | wc -l` != "0" ]; then \
		mv .new_prefix .prefix; \
	fi
	rm -f .new_prefix

version:
	echo "#define VERSION_REV \"$(HEAD)\"" > .new_version.h
	if [ ! -e version.h ] || [ `diff -u version.h .new_version.h | wc -l` != "0" ]; then \
		mv -f .new_version.h version.h; \
	fi
	rm -f .new_version.h

%.o: %.c
	@echo "  CC    $@"
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TESTS)
	@echo "  TEST  $^"
	lcov --directory . --zerocounters 2>/dev/null
	./run-tests 2>/dev/null

leaks: $(TESTS)
	@echo "  VALGR $^"
	valgrind --num-callers=20 --suppressions=../support/cnote.supp \
		 --leak-check=full ./sim.test

heap: $(TESTS)
	@echo "  VALGR $^"
	valgrind --tool=massif --time-unit=B ./sim.test

coverage: check
	@echo "  LCOV  $^"
	mkdir -p out
        # dont include unit test files in code coverage reports
	rm -f test_*.gc* handler-echo.gc*
	cd out; lcov --directory .. --capture --output-file app.info && genhtml app.info

# link the program
$(BINARY): $(OBJS) $(LIB_OBJS)
	@echo "  LD    $@"
	$(CC) -o $@ $^ $(LIBS) $(CFLAGS) $(LDFLAGS)

install: $(BINARY) cnote.service
	@echo "  INSTALL"
	install $(BINARY) $(PREFIX)/bin
	if [ -d  /lib/systemd/system ]; then \
		install cnote.service /lib/systemd/system; \
		systemctl --system daemon-reload; \
	fi
	ldconfig

doc: doxygen.config
	doxygen $^

cnote.service: cnote.service.in .prefix
	cat cnote.service.in | sed "s|PREFIX|$(PREFIX)|" >$@

%.test$(EXE_SUFFIX): test_%.o $(TEST_OBJS) $(LIB_OBJS)
	@echo "  LD    $@"
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS) $(LDFLAGS) -lcheck

# calculate C include
%.d: %.c
	@echo "  DEPS  $@"
	../support/depend.sh `dirname $*.c` $(CFLAGS) $*.c > $@

# include the C include dependencies
-include $(OBJS:.o=.d)
-include $(LIB_OBJS:.o=.d)

clean:
	find . -name "*.o" | xargs rm -f
	find . -name "*.gcno" | xargs rm -f
	find . -name "*.gcda" | xargs rm -f
	find . -name "*.gcov" | xargs rm -f
#	find test -name "ctx*" -type d | xargs rm -rf
	rm -rf test/ctxt*
	rm -f $(TARGETS)
	rm -f gmon.out
	rm -f version.h
	rm -f ./.prefix
	rm -f cnote.service
	rm -rf doxy

distclean: clean
	find . -name "*.d" | xargs rm -f
	find . -name "*.pb-c.*" | xargs rm -f
	find . -name "*~" | xargs rm -f
	rm -rf ./out

.PHONY: clean distclean check leaks coverage heap doc
