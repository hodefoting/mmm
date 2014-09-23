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

all: mmm.linux mmm.sdl

clean: clean-bins
clean-bins:
	rm -f mmm.sdl mmm.linux

mmm.sdl: bin/sdl*.c bin/host.c libmmm.a
	$(CC) -Ilib `pkg-config sdl --libs --cflags` bin/host.c bin/sdl*.c libmmm.a -o $@

mmm.linux: bin/host.c libmmm.a bin/linux*.c
	$(CC) -Ilib bin/host.c libmmm.a bin/linux*.c -o $@

install: install-bins
install-bins: mmm.sdl mmm.linux
	install mmm.sdl $(PREFIX)/bin
	install mmm.linux $(PREFIX)/bin
	install mmm $(PREFIX)/bin

