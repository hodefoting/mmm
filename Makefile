CC=gcc
LIB_PKGMODULES=cairo gtk+-3.0 sdl
PROJECT_NAME=mmfb
PROJECT_DESCRIPTION=mmfb
SYMBOL_PREFIX=
CFLAGS=-Wall -Wextra -O0

LIB_LD_FLAGS=-lutil -lm

LIB_CFILES=$(wildcard lib/*.c)
BIN_CFILES=$(wildcard bin/*.c)

INSTALLED_HEADERS=lib/mrg-config.h \
lib/mrg.h \
lib/mrg-events.h \
lib/mrg-style.h \
lib/mrg-text.h \
lib/mrg-util.h

include .mm/magic
include .mm/lib
include .mm/bin
include .mm/pkgconfig

$(BINARY).static: $(BIN_CFILES) $(LIBNAME_A)
	@echo "CCLD" $@; $(CC) $(SYSROOT) -Ilib -I .. $(BIN_CFLAGS) $(BIN_LD_FLAGS)  \
		$(BIN_CFILES) $(LIBNAME_A) -static -o $@ \
		    /usr/lib/i386-linux-gnu/libcairo.a  \
		    /usr/lib/i386-linux-gnu/libpixman-1.a \
		    /usr/lib/i386-linux-gnu/libpng12.a \
		    /usr/lib/i386-linux-gnu/libz.a \
		    /usr/lib/i386-linux-gnu/libfontconfig.a \
		    /usr/lib/i386-linux-gnu/libfreetype.a \
		    /usr/lib/i386-linux-gnu/libexpat.a \
		    /usr/lib/i386-linux-gnu/libutil.a -lpthread -lm

    # $(BIN_LD_FLAGS) -lpixman-1 -lpng -lz -lpthread -lm -lfreetype -lfontconfig -lexpat
