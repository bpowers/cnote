MODULES := lib src
include config.mk

CC=clang

# look for include files in each of the modules
CFLAGS += $(patsubst %,-I%,$(MODULES))
CFLAGS += -Iinclude -std=gnu99 -fblocks
# each module will add to this
SRC :=
# include the description for each module
include $(patsubst %,%/module.mk,$(MODULES))
# determine the object files
OBJ := $(patsubst %.c,%.o,$(filter %.c,$(SRC))) \
       $(patsubst %.y,%.o,$(filter %.y,$(SRC)))
# link the program
src/cnote: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LIBS) $(LDFLAGS)
# include the C include dependencies 
include $(OBJ:.o=.d)
# calculate C include
%.d: %.c
	support/depend.sh `dirname $*.c` $(CFLAGS) $*.c > $@


all: src/cnote
clean:
	find . -name "*.o" | xargs rm
	find . -name "*.d" | xargs rm

.PHONY: clean