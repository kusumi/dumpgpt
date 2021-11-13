UNAME := $(shell uname)

ifeq ($(UNAME), FreeBSD)
CC	= clang
else
CC	= gcc
endif

PROG	= dumpgpt
SRCS	= main.c gpt.c subr.c freebsd/uuid_to_string.c
CFLAGS	= -Wall -g

OBJS	= $(SRCS:.c=.o)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	if [ -f ./$(PROG) ]; then \
		rm ./$(PROG); \
	fi
	find . -type f -name "*.o" | xargs rm
