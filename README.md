Memory Mapped Machine
=====================

_mmm_ is a shared memory protocol for virtualising access to framebuffer
graphics, audio output and input event. The mmm project provides a C library
and a couple of sample hosts. Both clients and hosts can be statically linked,
thus permitting a small static binary to be used with hosts for multiple
different environments. Once the ABI is frozen; mmm clients could be a
convenient way to distribute stand-alone GUI applications, that only
rely on a framebuffer.

Features
--------

 - 32bit/pixel (resizable) framebuffer
 - PCM data output
    signed 16bit float and stereo
 - events
   - pointer events
   - utf8 keyboard events
   - messages from host to client
 - messages (perhaps rename to commands?)
   - free form; to send messages from client to host(s)
 - minimal dependencies

Desirable additions
-------------------

 - use of select instead of polling for events
 - add a minimal functional window manager to hosts
 - add support for more linux fbdev bpps
 - more hosts for more ui systems/platforms (drm, wayland)

API
---

The mmm library provides code for clients and hosts to create and interact
with such machines/files through mmap. The API is exposed through the [mmm.h
](../../blob/master/lib/mmm.h#L36) header.

A simple example that fills the framebuffer with some generated pixel values
is in the [examples/test.c](../../blob/master/examples/test.c) file. An even
simpler minimal client not using the mmm library is the
[examples/raw-client.c](../../blob/master/examples/raw-client.c) example, it
treats the shared memory as a peek and poke-able region with the
stable offsets of the data structure as addresses.

Architecture
------------

_mmm_ uses a client/host model - where a separate process is responsible for
displaying the graphics, playing/mixing the audio and gathering input events.

This split makes the clients independent of host specific libraries needed to
display (for X, wayland, SDL or similar) - only the host binary needs to have
links to these platform specific libraries, it is even possible to shut down
one host and later start another that interacts with the client(s).

The host and client communicate through a single file. The client is
responsible for creating, and growing, the file.  While the host is responsible
for deleting the file, when the pid of the child is no longer running.

If no host exists (*MMM\_PATH* environment variable is not set), the client
will spawn a host process for hosting itself - with the mmm command (a
shell-script that dispatches to different hosts, for easier
debugging/configuration).

Hosts
-----

The hosts supplied with mmm are, for now, only suitable for hosting a single
mmm client.

### Linux framebuffer

Directly interacts with standard input, _/dev/input/mice_ and _/dev/fb0_ or
_/dev/graphics/fb0_, a yields a small self self-contained binary that works
with 16,24 and 32 bit framebuffers.

### SDL 1.2

Permits running under a wide range of environments, linux X11, mac, linux
fbdev, wayland through X11.

### Third party

The API provided by libmmm is sufficient to implement multiplexing window
managers, which is what micro raptor gui uses for its client processes.
