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

all: mmfb.sdl

mmfb.sdl: bin/*.c libmmfb.so
	$(CC) -Ilib `pkg-config sdl --libs --cflags` bin/*.c libmmfb.so -o $@
