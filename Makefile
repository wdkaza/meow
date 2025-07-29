install:

	gcc meow.c -o meow -lX11

	cp meow /usr/bin/meow


clean:

	rm -f meow

	rm -f /usr/bin/meow``
