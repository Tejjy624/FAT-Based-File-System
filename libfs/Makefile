# Target library
lib 	:= libfs.a
objs 	:= fs.o disk.o

CC 		:= gcc
AR 		:= ar
CFLAGS 	:= -Wall -Wextra -g #-Werror
ARFLAGS := rcs

ifneq ($(V), 1)
Q = @
endif

all: $(lib)

$(lib): $(objs)
	@echo "AR $@"
	$(Q) $(AR) $(ARFLAGS) $@ $^
%.o: %.c %.h
	@echo "CC $@"
	$(Q) $(CC) $(CFLAGS) -c -o $@ $<
clean:
	@echo "CLEAN"
	$(Q) rm $(objs) *.a
