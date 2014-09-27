CC=gcc
LIB_PKGMODULES=
PROJECT_NAME=mmm
PROJECT_DESCRIPTION=Memory Mapped Machine
SYMBOL_PREFIX=
CFLAGS=-Wall -Wextra -O0

LIB_LD_FLAGS=-lutil -lm

LIB_CFILES=$(wildcard lib/*.c)

#BIN_CFILES=$(wildcard bin/*.c)

INSTALLED_HEADERS=lib/mmm.h

include .mm/magic
include .mm/lib
#include .mm/bin
include .mm/pkgconfig

all: mmm.linux mmm.static

clean: clean-bins
clean-bins:
	rm -f mmm.sdl mmm.linux mmm.static

mmm.sdl: bin/sdl*.c bin/host.c libmmm.a
	$(CC) -Ilib `pkg-config sdl --libs --cflags` bin/host.c bin/sdl*.c libmmm.a -o $@

mmm.linux: bin/host.c libmmm.a bin/linux*.c
	$(CC) -Ilib bin/host.c libmmm.a bin/linux*.c -o $@

install: install-bins
install-bins: mmm.linux
	install -d $(PREFIX)/bin
	install mmm.linux $(PREFIX)/bin
	install mmm.static $(PREFIX)/bin
	install mmm $(PREFIX)/bin

mmm.static: bin/*.c libmmm.a lib/*.h
	$(CC) -Os -static bin/linux*.c libmmm.a -Ilib bin/host*.c -o $@
	strip $@

dist:
	(cd ..;tar cvzf mmm.tar.gz --exclude='.git*'\
		 --exclude='mmm.static' \
		 --exclude='mmm.linux' \
		 --exclude='mmm.sdl' \
		 --exclude='todo' \
		 --exclude='*.o' \
		 --exclude='*.a' \
		 --exclude='*.so' \
	mmm; ls -sl mmm.tar.gz)
