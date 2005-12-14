# uoamhub Makefile
# (c) 2005 Max Kellermann <max@duempel.org>
# $Id$

CC := $(shell which gcc-4.0 gcc-3.4 gcc-3.3 gcc cc |grep -v '^no' |head -1 )

DEBUG = no

# change the comments to enable the non-debugging version
ifeq ($(DEBUG),yes)
CFLAGS += -g -O0
LDFLAGS = -g -O0
else
CFLAGS = -O3 -DNDEBUG=1
LDFLAGS = -O3
endif

#FEATURE_CFLAGS += -DDUMP_HEADERS -DDUMP_LOGIN
#FEATURE_CFLAGS += -DDUMP_SERVER_SEND -DDUMP_CLIENT_SEND
#FEATURE_CFLAGS += -DDUMP_SERVER_RECEIVE -DDUMP_CLIENT_RECEIVE
#FEATURE_CFLAGS += -DDUMP_SERVER_PEEK -DDUMP_CLIENT_PEEK
#FEATURE_CFLAGS += -DDUMP_WALK

ifeq ($(DEBUG),yes)
WARNING_CFLAGS += -W -Wall -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Waggregate-return -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef -pedantic-errors -Werror
else
WARNING_CFLAGS += -std=gnu99
endif

SOURCES = src/uoproxy.c src/config.c src/buffer.c src/sockbuff.c src/server.c src/client.c src/packets.c src/compression.c src/netutil.c src/instance.c src/daemon.c src/connection.c src/cnet.c src/cstate.c src/walk.c src/handler.c src/shandler.c src/chandler.c src/attach.c src/reconnect.c src/dump.c src/sutil.c src/command.c
HEADERS = $(wildcard src/*.h)

OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

all: src/uoproxy

clean:
	rm -f src/uoproxy src/*.o

$(OBJECTS): %.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $(WARNING_CFLAGS) $(FEATURE_CFLAGS) -o $@ $<

src/uoproxy: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

strip: src/uoproxy
	strip --strip-all $^
