.POSIX:
.PHONY: clean

CC           = cc
CFLAGS       = -O3
LDFLAGS      =
CURSES_LIBS  = -lncursesw
PTHREAD_LIBS = -lpthread
LDLIBS       = $(CURSES_LIBS) $(PTHREAD_LIBS)

# find src -name '*.c' | sort | sed 's/\.c/.o/' | tr '\n' ' '
OBJECTS = src/device.o src/i2c-packet.o src/interface.o src/interface-list.o src/main.o src/xalloc.o

main: $(OBJECTS)
	$(CC) -std=c99 $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

.c.o:
	$(CC) -std=c99 $(CFLAGS) -Isrc -D_XOPEN_SOURCE=500 -D_XOPEN_SOURCE_EXTENDED -c -o $@ $<

clean:
	rm -f main $(OBJECTS)
