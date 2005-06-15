CC := gcc-4.0

# change the comments to enable the non-debugging version
CFLAGS = -g -O0
#CFLAGS = -Os -DNDEBUG=1

WARNING_CFLAGS += -W -Wall -D_REENTRANT -std=gnu99 -Wmissing-prototypes -Wwrite-strings -Wcast-qual -Wfloat-equal -Wshadow -Wpointer-arith -Wbad-function-cast -Wsign-compare -Waggregate-return -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Wno-long-long -Wstrict-prototypes -Wundef -pedantic-errors -Werror

LDFLAGS =


all: uoproxy

clean:
	rm -f uoproxy

uoproxy: uoproxy.c
	$(CC) $(CFLAGS) $(WARNING_CFLAGS) $(LDFLAGS) -o $@ $^

strip: uoproxy
	strip --strip-all uoproxy
