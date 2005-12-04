CC := gcc-4.0

# change the comments to enable the non-debugging version
CFLAGS += -g -O0
#CFLAGS = -Os -DNDEBUG=1
LDFLAGS = -g -O0

#CFLAGS += -DDUMP_SERVER_SEND -DDUMP_CLIENT_SEND

WARNING_CFLAGS += -W -Wall -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Waggregate-return -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef -pedantic-errors -Werror

SOURCES = src/uoproxy.c src/buffer.c src/sockbuff.c src/server.c src/client.c src/packets.c src/compression.c src/netutil.c src/connection.c src/handler.c src/shandler.c src/chandler.c src/attach.c src/dump.c src/sutil.c
HEADERS = $(wildcard src/*.h)

OBJECTS = $(patsubst %.c,%.o,$(SOURCES))

all: src/uoproxy

clean:
	rm -f src/uoproxy src/*.o

$(OBJECTS): %.o: %.c $(HEADERS)
	$(CC) -c $(CFLAGS) $(WARNING_CFLAGS) -o $@ $<

src/uoproxy: $(OBJECTS)
	$(CC) $(LDFLAGS) $(LDFLAGS) -o $@ $^

strip: src/uoproxy
	strip --strip-all $^
