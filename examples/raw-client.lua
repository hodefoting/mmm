#!/usr/bin/env luajit

-- direct lua implementation of a minimal raw fb client

local S   = require"syscall"
local ffi = require"ffi"
local WIDTH              = 800
local HEIGHT             = 600 
local BPP                = 4
local MMM_PID            = 0x18
local MMM_TITLE          = 0xb0
local MMM_WIDTH          = 0x5b0
local MMM_HEIGHT         = 0x5b4
local MMM_DESIRED_WIDTH  = 0x5d8
local MMM_DESIRED_HEIGHT = 0x5dc
local MMM_DAMAGE_WIDTH   = 0x5e8
local MMM_DAMAGE_HEIGHT  = 0x5ec
local MMM_STRIDE         = 0x5b8
local MMM_FB_OFFSET      = 0x5bc
local MMM_FLIP_STATE     = 0x5c0
local MMM_SIZE           = 0x48c70
local MMM_FLIP_INIT      = 0
local MMM_FLIP_NEUTRAL   = 1
local MMM_FLIP_DRAWING   = 2
local MMM_FLIP_WAIT_FLIP = 3
local MMM_FLIP_FLIPPING  = 4

local path = S.getenv ("MMM_PATH") .. '/' .. 'fb.' .. S.getpid();
local fd   = S.open(path, "RDWR, CREAT", "RUSR, WUSR, RGRP, ROTH")
local size = MMM_SIZE + WIDTH * HEIGHT * BPP
local map  = S.mmap(ffi.cast("void*", 4 * 1024 * 1024 * 1024), size, "READ, WRITE", "SHARED", fd, 0)

S.pwrite(fd, "", 1, size)

local bar = ffi.cast("int32_t*", map)
local baz = ffi.cast("int8_t*", map)

function PEEK(addr)
  return ffi.cast("int32_t*", ffi.cast("int8_t*", map) + addr)[0]
end

function POKE(addr, val)
  ffi.cast("int32_t*", ffi.cast("int8_t*", map) + addr)[0]=val
end

POKE(MMM_FLIP_STATE, MMM_FLIP_INIT)
POKE(MMM_PID, S.getpid())
POKE(MMM_WIDTH, WIDTH)
POKE(MMM_HEIGHT, HEIGHT)
POKE(MMM_DESIRED_WIDTH, WIDTH)
POKE(MMM_DESIRED_HEIGHT, HEIGHT)
POKE(MMM_STRIDE, WIDTH * BPP)
POKE(MMM_FB_OFFSET, MMM_SIZE)
POKE(MMM_FLIP_STATE, MMM_FLIP_NEUTRAL)

ffi.cdef "int usleep(int seconds);"
print(path)
for f=0,1000,1 do
  while (PEEK(MMM_FLIP_STATE) ~= MMM_FLIP_NEUTRAL) do
    ffi.C.usleep(5000)
  end
  POKE(MMM_FLIP_STATE, MMM_FLIP_DRAWING)
  for x=0,WIDTH-1,1 do
    for y=0,HEIGHT-1,1 do
    POKE(MMM_SIZE + y * WIDTH * BPP + x * 4, x * y + f)
  end
  end

  POKE (MMM_DAMAGE_WIDTH,  PEEK (MMM_WIDTH))
  POKE (MMM_DAMAGE_HEIGHT, PEEK (MMM_HEIGHT))
  POKE(MMM_FLIP_STATE, MMM_FLIP_WAIT_FLIP)
end

