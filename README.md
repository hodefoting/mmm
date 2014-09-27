Memory Mapped Machine
=====================

_mmm_ is a shared memory protocol for virtualising access to framebuffer
graphics, audio output and input event. The mmm project provides a C library
and a couple of sample hosts. 

Features
--------

 - 32bit/pixel (resizable) framebuffer
 - PCM data output
 - events
   - pointer events
   - utf8 keyboard events
 - messages (perhaps rename to commands?)
   - free form; to communicate with host(s)
 - minimal dependency

Desirable additions
-------------------

 - use of select instead of polling for events
 - add a minimal functional window manager to hosts
 - add support for more linux fbdev bpps
 - improved and more hosts for more ui systems/platforms (kms, wayland, mir, ..)

API
---

The mmm library provides code for clients and hosts to create and interact
with such machines/files through mmap. The API is exposed through the [mmm.h
](../../blob/master/lib/mmm.h#L36) header.

A simple example that fills the framebuffer with some generated pixel values
is in the [examples/test.c](../../blob/master/examples/test.c) file. An even
simpler minimal client not using the mmm library is the
[examples/raw-client.c](../../blob/master/examples/raw-client.c) example, it
treats the shared memory as a peek and poke-able region with hard-coded
addresses.

Architecture
------------

_mmm_ uses a client/host model - where a separate process is responsible for
displaying the graphics, playing/mixing the audio and gathering input events.

This split makes the clients independent of host specific libraries needed to
display (for X, wayland, SDL or similar) - only the host binary needs to have
links to these platform specific libraries.

The host and client communicate through a single file. The client is
responsible for creating, and growing, the file.  While the host is
responsible for deleting the file.

If no host exists (*MMM\_PATH* environment variable is not set), the client
will try to spawn a host process.

Hosts
-----

The hosts supplied with mmm are, for now, only suitable for hosting a single
mmm client.

### Linux framebuffer

Directly interacts with standard input, _/dev/input/mice_ and _/dev/fb0_ or
_/dev/graphics/fb0_, a tiny self-contained binary which works with 16,24 and
32 bit framebuffers.

### SDL 1.2

Permits running under a wide range of environments, linux X11, mac, linux
fbdev.

### Third party

The API provided by libmmm is sufficient to implement multiplexing window
managers and more.
