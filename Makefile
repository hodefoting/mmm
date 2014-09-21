CC=gcc
LIB_PKGMODULES=sdl
PROJECT_NAME=mmfb
PROJECT_DESCRIPTION=mmfb
SYMBOL_PREFIX=
CFLAGS=-Wall -Wextra -O0

LIB_LD_FLAGS=-lutil -lm

LIB_CFILES=$(wildcard lib/*.c)
BIN_CFILES=$(wildcard bin/*.c)

INSTALLED_HEADERS=lib/mmfb.h

include .mm/magic
include .mm/lib
include .mm/bin
include .mm/pkgconfig
