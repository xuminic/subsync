
ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif


all:
	gcc -Wall -O3 -o subsync subsync.c

clean:
	rm -f subsync

install: subsync
	install -s subsync $(PREFIX)/bin
	install -d $(PREFIX)/share/man/man1
	install -m 644 subsync.1 $(PREFIX)/share/man/man1

uninstall:
	rm -f $(PREFIX)/bin/subsync $(PREFIX)/share/man/man1/subsync.1
