CC=gcc
LIB_PKGMODULES=
PROJECT_NAME=mmfb
PROJECT_DESCRIPTION=mmfb
SYMBOL_PREFIX=
CFLAGS=-Wall -Wextra -O0

LIB_LD_FLAGS=-lutil -lm

LIB_CFILES=$(wildcard lib/*.c)

#BIN_CFILES=$(wildcard bin/*.c)

INSTALLED_HEADERS=lib/mmfb.h

include .mm/magic
include .mm/lib
#include .mm/bin
include .mm/pkgconfig

all: mmfb.linux mmfb.sdl

clean: clean-bins
clean-bins:
	rm -f mmfb.sdl mmfb.linux

mmfb.sdl: bin/sdl.c bin/host.c libmmfb.a
	$(CC) -Ilib `pkg-config sdl --libs --cflags` bin/host.c bin/sdl.c libmmfb.a -o $@

mmfb.linux: bin/linux.c bin/host.c libmmfb.a
	$(CC) -Ilib bin/host.c bin/linux.c libmmfb.a -o $@
