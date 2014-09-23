Memory Mapped Machine
=====================

*mmm* is a C library API providing access to virtual input, graphics and audio
through a shared memory (mmaped file) interface.

Interfaces implemented
----------------------

- 32bit/pixel framebuffer (resizable)
- pcm data output
- event input stream
  keyboard / mouse input
- message output stream

Client/Host model
-----------------

mmm uses a client/host model - where a separate process is responsible for
displaying the graphics, playing/mixing the audio and gathering input events.

The host and client communicate through a single file. The client is
responsible for creating, and growing, the file.  While the host is
responsible for deleting the file. If no host exists (MMM\_PATH environment
variable is not set), the client will try to spawn a child process host.

The mmm library provides code for clients and hosts to create and interact
with such machines/files through mmap. The API is exposed through the [mmm.h
](../../blob/master/lib/mmm.h#L1) header.

host backends
-------------

### Linux framebuffer

Directly interacts with /dev/input/mice and /dev/fb0 or /dev/graphics/fb0,
provides a tiny binary size and code that works with 32bits per pixel and
16bits per pixel framebuffers.

### SDL 1.2

Permits running under a wide range of environments, linux X11, mac, linux
fbdev.

## Portability

As long as mmap works, it should work fine; it might be possible to adapt the
code to also work on win32. The code has thus far only been tested with glibc
and musl; but it shouldn't be hard to make more portable.

