CC := gcc-4.0

# change the comments to enable the non-debugging version
CFLAGS = -g -O0
#CFLAGS = -Os -DNDEBUG=1

WARNING_CFLAGS += -W -Wall -D_REENTRANT -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Waggregate-return -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef -pedantic-errors -Werror

LDFLAGS =


all: src/uoproxy

clean:
	rm -f src/uoproxy

src/uoproxy: src/uoproxy.c src/buffer.c src/sockbuff.c src/packets.c src/compression.c src/netutil.c
	$(CC) $(CFLAGS) $(WARNING_CFLAGS) $(LDFLAGS) -o $@ $^

strip: src/uoproxy
	strip --strip-all $^
