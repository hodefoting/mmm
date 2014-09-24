Memory Mapped Machine
=====================

_mmm_ is a C library API providing access to virtual input, graphics and audio
through a shared memory (mmaped file) interface.

Features
--------

- 32bit/pixel framebuffer (resizable)
- PCM data output
- pointer events
- utf8 keyboard events
- message output stream

Client/Host model
-----------------

_mmm_ uses a client/host model - where a separate process is responsible for
displaying the graphics, playing/mixing the audio and gathering input events.

This split makes the clients independent of host specific libraries needed to
display (for X, wayland, SDL or similar) - only the host binary needs to have
links to these platform specific libraries.

The host and client communicate through a single file. The client is
responsible for creating, and growing, the file.  While the host is
responsible for deleting the file. If no host exists (*MMM\_PATH* environment
variable is not set), the client will try to spawn a child process host.

The mmm library provides code for clients and hosts to create and interact
with such machines/files through mmap. The API is exposed through the [mmm.h
](../../blob/master/lib/mmm.h#L36) header.

host backends
-------------

### Linux framebuffer

Directly interacts with _/dev/input/mice_ and _/dev/fb0_ or _/dev/graphics/fb0_,
provides a tiny binary size and code that works with 32bits per pixel and
16bits per pixel framebuffers.

### SDL 1.2

Permits running under a wide range of environments, linux X11, mac, linux
fbdev.

### more

As the way hosts are written gets better abstracted, creating wayland, mir,
android, ios, ?bsd etc hosts should be easy, and thus provide wider
portability for mmm apps.

## Portability

As long as mmap works, it should work fine; it might be possible to adapt the
code to also work on win32. The code has thus far only been tested with glibc
and musl; but it shouldn't be hard to make more portable.
