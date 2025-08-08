CC      = gcc
CFLAGS  = -Wall -Wextra -I/usr/include/freetype2 $(shell pkg-config --cflags xft x11)
LDFLAGS = $(shell pkg-config --libs xft x11)

install: meow
	cp meow /usr/bin/meow

meow: meow.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f meow
	rm -f /usr/bin/meow

# weird bug with ft2build.h need to use this so it compiles
