MODULES := lib src

include config.mk

# if you invoke make as 'make V=1' it will verbosely list what it is doing,
# otherwise it defaults to pretty mode, which makes errors _much_ easier to see
ifneq ($V, 1)
MAKEFLAGS = -s
endif

WARNFLAGS := \
        -Wformat -Wall -Wundef -Wpointer-arith -Wcast-qual \
        -Wwrite-strings -Wsign-compare -Wmissing-noreturn \
        -Wextra -Wstrict-aliasing=2
# clang doesn't know about this yet
#        -Wunsafe-loop-optimizations

# look for include files in each of the modules
CFLAGS += $(patsubst %,-I%,$(MODULES)) -I.
CFLAGS += -std=gnu99 -Iinclude $(WARNFLAGS)
# each module will add to this
SRC :=
# include the description for each module
include $(patsubst %,%/module.mk,$(MODULES))
# determine the object files
OBJ := $(patsubst %.c,%.o,$(filter %.c,$(SRC))) \
       $(patsubst %.y,%.o,$(filter %.y,$(SRC)))

TESTS_SRC := $(shell find test -name 'test_*.c')
TESTS := $(patsubst test/test_%.c,test/%.test,$(TESTS_SRC))

TARGETS := src/cnote

all: $(TESTS) $(TARGETS)

# clear out all suffixes
.SUFFIXES:
# list only those we use
.SUFFIXES: .d .c .o .test

# calculate C include
%.d: %.c
	@echo "  DEPS  $@"
	support/depend.sh `dirname $*.c` $(CFLAGS) $*.c > $@

%.o: %.c
	@echo "  CC    $@"
	$(CC) -c -o $@ $< $(CFLAGS)

# include the C include dependencies
-include $(OBJ:.o=.d)
-include $(TESTS_SRC:.c=.d)

# link the program
src/cnote: $(OBJ)
	@echo "  LD    $@"
	$(CC) -o $@ $(OBJ) $(LIBS) $(CFLAGS) $(LDFLAGS)

test/%.test: test/test_%.o
	@echo "  LD    $(TESTS)"
	$(CC) -o $@ $< $(LIBS) $(CFLAGS) $(LDFLAGS) -lcheck

clean:
	find . -name "*.o" | xargs rm -f
	rm -f $(TARGETS) $(TESTS)

distclean: clean
	find . -name "*.d" | xargs rm -f

.PHONY: clean distclean
