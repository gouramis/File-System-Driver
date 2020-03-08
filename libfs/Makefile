# Target library
lib := libfs.a

all: $(lib)

objs := fs.o \
		disk.o
# General gcc options
CC := gcc
CFLAGS	:= -Wall -Wextra -Werror
# Verbose flag
ifneq ($(V),1)
Q = @
V = 0
endif
# Debug Flag
ifneq ($(D),1)
CFLAGS += -O2
else
CFLAGS += -g
endif
# Include dependencies
deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
# Generate dependencies
DEPFLAGS = -MMD -MF $(@:.o=.d)
# Rule for libuthread.a
%.a: $(objs)
	@echo "CC $@"
	$(Q)ar rcs $@ $^
# Generic rule for compiling objects
%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)
# Cleaning rule
clean:
	@echo "CLEAN"
	$(Q)rm -f $(objs) $(deps) $(lib)
