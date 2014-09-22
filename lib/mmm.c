/*
 * 2014 (c) Øyvind Kolås
Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "mmm.h"

#define MMM_WAIT_ATTEMPTS 30
#define USE_ATOMIC_OPS 1

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/stat.h>

#define MMM_MAX_EVENT  1024

typedef enum {
  MMM_INITIALIZING = 0,
  MMM_NEUTRAL,
  MMM_DRAWING,
  MMM_WAIT_FLIP,
  MMM_FLIPPING,
} MmmFlipState;

typedef struct _MmmShm MmmShm;

#define MMM_AUDIO_BUFFER_SIZE  4096

#define U64_CONSTANT(str) (*((uint64_t*)str))

static char *MMM_magic    = "MMM ░ ";
static char *MMM_fb       = "FB      ";
static char *MMM_events   = "EVENTS  ";
static char *MMM_messages = "MESSAGES";
static char *MMM_pcm      = "PCM     ";
static char *MMM_fbdata   = "FBDATA  ";

typedef struct MmmBlock {
  uint64_t type;
  uint32_t length;
  uint32_t next;
  uint8_t  data[];
} MmmBlock;

/* In the comments, a C means the client (generally) writes and a H means the
 * host (generally) writes,
 */

typedef struct MmmHeader {
 MmmBlock   block;

 uint32_t   client_version;  /* */
 uint32_t   server_version;  /* */
 int32_t    pid;             /* C (by _convention_ 64bit systems also have 32bit pids)*/
 int        lock;            /* */
 /* revision?                   */
 /* flags.. */
 uint32_t   pad[32];
} MmmHeader;

typedef struct MmmFb {
 MmmBlock   block;
 uint8_t    title[256];      /* C  window title  */ /* XXX: need generic properties?  */
 uint8_t   *babl_format[128];/* C  pixel format; according to babls conventions */
 int32_t    width;           /* C  width of raster in pixels */
 int32_t    height;          /* C  height of raster in pixels */
 int32_t    stride;          /* C  byte offset between starts of */
                                   /* pixelrows in memory   */
 int32_t    fb_offset;       /* offset in file where fb is located */

 int32_t    flip_state;      /* CH used for synchronising flips  */

 double     x;               /* CH it isn't certain that the server  */
 double     y;               /* CH abides by these coordinates       */
 int32_t    desired_width;   /* HC used for initiating resizes   0 from client mean non-resizable */
 int32_t    desired_height;  /* HC shm makes this be correct   */

 int32_t    damage_x;        /* CH */
 int32_t    damage_y;        /* CH */
 int32_t    damage_width;    /* CH */
 int32_t    damage_height;   /* CH */
 uint32_t   pad[32];
} MmmFb;

typedef struct MmmEvents {
  MmmBlock       block;
  int16_t        read;      /* C  last event_no which has been read    */
  int16_t        write;     /* H  last event_no which has been written */
  uint32_t       pad[8];
  uint8_t        buffer[MMM_MAX_EVENT][128]; /* S circular list of events    */
} MmmEvents;

typedef struct MmmMessages {
  MmmBlock       block;
  int16_t        read;      /* H  last message_no which has been read    */
  int16_t        write;     /* C  last message_no which has been written */
  uint32_t       pad[8];
  uint8_t        buffer[MMM_MAX_EVENT][128]; /* C circular list of events    */
} MmmMessages;

typedef struct MmmPcm {
  MmmBlock       block;
  MmmAudioFormat format;                        /* C */
  int            sample_rate;                   /* C */ 
  int            host_sample_rate;              /* H */
  int            read;                          /* H */
  int            write;                         /* C */
  uint32_t       pad[16];
  uint8_t        buffer[MMM_AUDIO_BUFFER_SIZE]; /* C */
} MmmPcm;

struct  _MmmShm { 
  MmmHeader      header;    /* must be first  in file */
  MmmFb          fb;        /* must be second in file */
  MmmEvents      events;    /* must be third  in file */
  MmmMessages    messages;  /* must be fourth in file */
  MmmPcm         pcm;       /* must be fifth  in file */

  /* ... potential new blocks  ... */

  MmmBlock       pixeldata; /* offset for pixeldata is defined in fb */
};

static Mmm *mmm_new_shm (int width, int height, void *babl_format);

struct Mmm_
{
  int          bpp;      /* do not move, inline function in header ..*/
  int          stride;   /* .. depends on position of bpp/stride */

  uint8_t     *fb;       /* pointer to actual pixels */
  int          width;
  int          height;
  int          mapped_size;

  void        *format;   /* babl format */
                         /* XXX: add format for frontbuffer*/

  char        *path;
  int          fd;

  MmmShm      *shm;

  int          compositor_side;

  MmmBlock    *pcm;
  MmmBlock    *events;
  MmmBlock    *messages;
};

static void mmm_remap (Mmm *fb);

int mmm_get_bytes_per_pixel (Mmm *fb)
{
  return fb->bpp;
}

static struct timeval start_time;

static void mmm_init_ticks (void)
{
  static int done = 0;

  if (done)
    return;
  done = 1;
  gettimeofday (&start_time, NULL);
}

long mmm_ticks (void)
{
  struct timeval measure_time;
  mmm_init_ticks ();
  gettimeofday (&measure_time, NULL);
#define usecs(time) ((time.tv_sec - start_time.tv_sec) * 1000000 + time.tv_usec)
  return usecs (measure_time) - usecs (start_time);
#undef usecs
}

int
mmm_wait_neutral (Mmm *fb)
{
  int attempts = MMM_WAIT_ATTEMPTS;

  while (fb->shm->fb.flip_state != MMM_NEUTRAL && --attempts)
    usleep (500);

  return (attempts > 0 ? 0 : -1);
} 

static int
mmm_set_state (Mmm *fb, MmmFlipState state)
{
  fb->shm->fb.flip_state = state;
  return 1;
}

unsigned char *
mmm_get_buffer_write (Mmm *fb, int *width, int *height, int *stride,
    void *babl_format)
{
  mmm_wait_neutral (fb);
  //fprintf (stderr, "[%i]", fb->bpp);

  mmm_set_state (fb, MMM_DRAWING);
  if (width) *width = fb->width;
  if (height) *height = fb->height;
  if (stride) *stride = fb->stride;

  assert (fb->fb);
  return fb->fb;
}

static inline void memcpy32_16 (uint8_t *dst, const uint8_t *src, int count)
{
  while (count--)
    {
      int big = ((src[0] >> 3)) +
                ((src[1] >> 2)<<5) +
                ((src[2] >> 3)<<11);
      dst[1] = big >> 8;
      dst[0] = big & 255;
      dst+=2;
      src+=4;
    }
}

void _mmm_get_coords (Mmm *mmm, double *x, double *y);

void
mmm_write_done (Mmm *fb, int x, int y, int width, int height)
{
  if (width == 0 && height == 0)
    {
      /* nothing written */
      fb->shm->fb.flip_state = MMM_NEUTRAL;
      return;
    }
  fb->shm->fb.flip_state = MMM_WAIT_FLIP;

  if (width <= 0)
  {
    fb->shm->fb.damage_x = 0;
    fb->shm->fb.damage_y = 0;
    fb->shm->fb.damage_width = fb->shm->fb.width;
    fb->shm->fb.damage_height = fb->shm->fb.height;
  }
  else
  {
    /* XXX: should combine with existing damage */
    fb->shm->fb.damage_x = x;
    fb->shm->fb.damage_y = y;
    fb->shm->fb.damage_width = width;
    fb->shm->fb.damage_height = height;
  }

  fb->shm->fb.flip_state = MMM_WAIT_FLIP;
}

int
mmm_wait_neutral_or_wait_flip (Mmm *fb)
{
  int attempts = MMM_WAIT_ATTEMPTS;

  while (fb->shm->fb.flip_state != MMM_NEUTRAL &&
         fb->shm->fb.flip_state != MMM_WAIT_FLIP &&
         --attempts)
    usleep (500);

  if (attempts < 1)
    fprintf (stderr, "eeew\n");

  return   (attempts > 0 ? 0 : -1 );
}

const unsigned char *
mmm_get_buffer_read (Mmm *fb, int *width, int *height, int *stride)
{
  if (width)  *width  = fb->width;
  if (height) *height = fb->height;

  if(mmm_host_check_size (fb, NULL, NULL))
    return NULL;
  if (mmm_wait_neutral_or_wait_flip (fb))
    return NULL;

  if (stride) *stride = fb->stride;

  mmm_set_state (fb, MMM_FLIPPING);

  return (void*)fb->fb; /* the direct device XXX: should use double buffering for toplevel */
}

void
mmm_read_done (Mmm *fb)
{
  fb->shm->fb.damage_x = 0;
  fb->shm->fb.damage_y = 0;
  fb->shm->fb.damage_width = 0;
  fb->shm->fb.damage_height = 0;
  fb->shm->fb.flip_state = MMM_NEUTRAL;
}

Mmm *
mmm_host_open (const char *path)
{
  Mmm *fb = calloc (sizeof (Mmm), 1);

  fb->compositor_side = 1;
  fb->fd = open (path, O_RDWR);
  if (fb->fd == -1)
    {
      free (fb);
      return NULL;
    }
  
  /* first we map just the MmmShm struct */
  fb->shm = mmap (NULL, sizeof (MmmShm), PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
  fb->mapped_size = sizeof (MmmShm);
  fb->bpp = 4;
  fb->width = fb->shm->fb.width;
  fb->height = fb->shm->fb.height;
  fb->stride = fb->shm->fb.width * fb->bpp;

  mmm_remap (fb);
  return fb;
}
static void mmm_init_header (MmmShm *shm);

static void
mmm_remap (Mmm *fb)
{
  {
    int size = sizeof(MmmShm) + fb->shm->fb.height * fb->shm->fb.stride;
    if (size > fb->mapped_size)
      {
        if (pwrite (fb->fd, "", 1, size+1) == -1)
          fprintf (stderr, "mmm failed stretching\n");
        fsync (fb->fd);
#if 0
        fb->shm = mremap (fb->shm, fb->mapped_size, size, MREMAP_MAYMOVE);
#else
        munmap (fb->shm, fb->mapped_size);
        fb->shm = mmap (NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fb->fd, 0);
#endif
        if (!fb->shm)
          fprintf (stderr, "eeeek!\n");
        fb->mapped_size = size;
      }
    if (!fb->compositor_side)
      mmm_init_header (fb->shm);
  }

  fb->width  = fb->shm->fb.width;
  fb->height = fb->shm->fb.height;
  fb->stride = fb->shm->fb.stride = fb->width * fb->bpp;
  fb->fb = ((uint8_t*)fb->shm) + fb->shm->fb.fb_offset;
}

int mmm_get_width  (Mmm *fb)
{
  return fb->width;
}
int mmm_get_height (Mmm *fb)
{
  return fb->height;
}

void
mmm_get_size (Mmm *fb, int *width, int *height)
{
  if (width)
    *width = fb->width;
  if (height)
    *height = fb->height;
}

int
mmm_host_check_size (Mmm *fb, int *width, int *height)
{
  int ret = 0;
  if (fb->width != fb->shm->fb.width ||
      fb->height != fb->shm->fb.height)
    {
      mmm_remap (fb);
      ret = 1;
      fb->stride = fb->shm->fb.stride;
      fb->width = fb->shm->fb.width;
    }
  if (width || height)
    mmm_get_size (fb, width, height);
  return ret;
}

int
mmm_client_check_size (Mmm *fb, int *width, int *height)
{
  int ret = 0;
  if (fb->shm->fb.desired_width  != fb->shm->fb.width ||
      fb->shm->fb.desired_height != fb->shm->fb.height)
    {
      mmm_set_size (fb, fb->shm->fb.desired_width, fb->shm->fb.desired_height);
      ret = 1;
    }
  if (width || height)
    mmm_get_size (fb, width, height);
  return ret;
}

void mmm_set_size (Mmm *fb, int width, int height)
{
  while ((fb->shm->fb.flip_state != MMM_NEUTRAL) &&
         (fb->shm->fb.flip_state != MMM_INITIALIZING))
    usleep (500);
  fb->shm->fb.flip_state = MMM_INITIALIZING;

  fb->shm->fb.width = fb->shm->fb.desired_width =  width;
  fb->shm->fb.height = fb->shm->fb.desired_height = height;
  fb->shm->fb.stride = fb->shm->fb.width * fb->bpp;
  mmm_remap (fb);
  fb->shm->fb.flip_state = MMM_NEUTRAL;
}

int mmm_has_event (Mmm *fb)
{
  if (fb->shm->events.read != fb->shm->events.write)
    return 1;
  return 0;
}

void mmm_add_event (Mmm *fb, const char *event)
{
  MmmShm *shm = fb->shm;
  int event_no = shm->events.write + 1;
  if (event_no >= MMM_MAX_EVENT)
    event_no = 0;

  if (event_no == shm->events.read)
    {
      static int once = 0;
      if (!once)
        fprintf (stderr, "oc event queue overflow\n");
      once = 1;
      return;
    }

  strcpy ((void*)shm->events.buffer[event_no], event);

  shm->events.write ++;
  if (shm->events.write >= MMM_MAX_EVENT)
    shm->events.write = 0;
}


const char *mmm_get_event (Mmm *fb)
{
  if (fb->shm->events.read != fb->shm->events.write)
    {
      fb->shm->events.read++;
      if (fb->shm->events.read >= MMM_MAX_EVENT)
        fb->shm->events.read = 0;
      return (void*)fb->shm->events.buffer[fb->shm->events.read];
    }
  return NULL;
}

Mmm *mmm_new (int width, int height, MmmFlag flags, void *babl_format)
{
  Mmm *fb = NULL;

  if (!getenv ("MMM_PATH"))
  {
    switch (fork())
    {
      case 0: /* child */
        execlp ("mmm", "mmm", NULL);
      case -1:
        fprintf (stderr, "fork failed\n");
        return 0;
    }
    setenv ("MMM_PATH", "/tmp/mmm", 1);
  }

  {
    int is_compositor = (getenv ("MMM_COMPOSITOR") != NULL);
    const char *env = getenv ("MMM_PATH");
    if (env && !is_compositor)
    {
      fb = mmm_new_shm (width, height, babl_format);
      mmm_set_size (fb, width, height);
    }
  }

  if (!fb)
  { 
    fprintf (stderr, "failed to initialize framebuffer\n");
    exit (-1);
  }

  if (fb)
    {
      mmm_set_title (fb, "mmm");
    }

  if (!fb)
    {
      fprintf (stderr, "unable to get framebuffer\n");
    }

  mmm_pcm_set_sample_rate (fb, 44100);
  mmm_pcm_set_format (fb, MMM_s16);

  return fb;
}

#include <assert.h>

static void mmm_init_header (MmmShm *shm)
{
  int pos = 0;
  int length;

  shm->fb.fb_offset = sizeof (MmmShm);
  length = sizeof (MmmHeader);
  shm->header.block.length = length;
  pos += length;
  shm->header.block.next = pos;
  assert (strlen (MMM_magic) == 8);
  memcpy (&shm->header.block.type, MMM_magic, 8);

  length = sizeof(MmmFb);
  shm->fb.block.length = length;
  pos += length;
  shm->fb.block.next = pos;
  assert (strlen (MMM_fb) == 8);
  memcpy (&shm->fb.block.type, MMM_fb, 8);

  length = sizeof (MmmEvents);
  shm->events.block.length = length;
  pos += length;
  shm->events.block.next = pos;
  assert (strlen (MMM_events) == 8);
  memcpy (&shm->events.block.type, MMM_events, 8);

  length = sizeof (MmmMessages);
  shm->messages.block.length = length;
  pos += length;
  shm->messages.block.next = pos;
  assert (strlen (MMM_messages) == 8);
  memcpy (&shm->messages.block.type, MMM_messages, 8);

  length = sizeof (MmmPcm);
  shm->pcm.block.length = length;
  pos += length;
  shm->pcm.block.next = pos;
  assert (strlen (MMM_pcm) == 8);
  memcpy (&shm->pcm.block.type, MMM_pcm, 8);

  assert (strlen (MMM_fbdata) == 8);
  memcpy (&shm->pixeldata.type, MMM_fbdata, 8);
}

static Mmm *mmm_new_shm (int width, int height, void *babl_format)
{
  Mmm *fb = calloc (sizeof (Mmm), 1);
  if (width < 0 && height < 0)
    {
      width = 640;
      height = 480;
    }

  fb->format = babl_format;
  fb->width  = width;
  fb->height = height;
  fb->bpp = 4;
  fb->stride = fb->width * fb->bpp;
  if (fb->path)
    free (fb->path);
  {
    char buf[512];
    const char *mmm_path = getenv("MMM_PATH");
    if (!mmm_path) mmm_path = "/tmp";
    sprintf (buf, "%s/fb.XXXXXX", mmm_path);
    fb->path = strdup (buf);
  }
  fprintf (stderr, "%s\n", fb->path);
  fb->fd = mkstemp (fb->path);
  pwrite (fb->fd, "", 1, sizeof (MmmShm) + fb->stride * fb->height);
  fsync (fb->fd);

  chmod (fb->path, 511);

  fb->mapped_size = fb->stride * fb->height + sizeof (MmmShm);
  fb->shm = mmap (NULL, fb->mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
  mmm_init_header (fb->shm);

  fb->shm->fb.desired_width = fb->width;
  fb->shm->fb.desired_height = fb->height;
  fb->shm->fb.width = fb->width;
  fb->shm->fb.stride = fb->stride;
  fb->shm->fb.height = fb->height;
  fb->shm->fb.flip_state = MMM_NEUTRAL;
  fb->shm->header.pid = getpid ();
  fprintf (stderr, "pidset\n");
  mmm_remap (fb);

  return fb;
}

void
mmm_destroy (Mmm *fb)
{
  munmap (fb->shm, fb->mapped_size);
  if (fb->fd)
    close (fb->fd);
  free (fb);
}

int mmm_get_damage (Mmm *fb, int *x, int *y, int *width, int *height)
{
  if (x)
    *x = fb->shm->fb.damage_x;
  if (y)
    *y = fb->shm->fb.damage_y;
  if (width)
    *width = fb->shm->fb.damage_width;
  if (height)
    *height = fb->shm->fb.damage_height;
  return fb->shm->fb.flip_state == MMM_WAIT_FLIP;
}

const char *mmm_get_babl_format (Mmm *fb)
{
  return (char *)fb->shm->fb.babl_format;
}

void
mmm_set_title (Mmm *mmm, const char *title)
{
  strncpy ((void*)mmm->shm->fb.title, title, 512);
}

const char *
mmm_get_title (Mmm *mmm)
{
  return (void*)mmm->shm->fb.title;
}

void mmm_set_x (Mmm *fb, int x)
{
  fb->shm->fb.x = x;
}

void mmm_set_y (Mmm *fb, int y)
{
  fb->shm->fb.y = y;
}

int  mmm_get_x (Mmm *fb)
{
  return fb->shm->fb.x;
}

int mmm_get_y (Mmm *fb)
{
  return fb->shm->fb.y;
}

long mmm_client_pid (Mmm *fb)
{
  return fb->shm->header.pid;
}

void mmm_host_get_size (Mmm *fb, int *width, int *height)
{
  if (!fb)
    return;
  if (width)
    *width = fb->shm->fb.desired_width;
  if (height)
    *height = fb->shm->fb.desired_height;
}

void mmm_host_set_size (Mmm *fb, int width, int height)
{
  if (!fb)
    return;
  fb->shm->fb.desired_width = width;
  fb->shm->fb.desired_height = height;
}

void mmm_pcm_set_sample_rate (Mmm *fb, int freq)
{
  fb->shm->pcm.sample_rate = freq;
}

int mmm_pcm_get_sample_rate (Mmm *fb)
{
  return fb->shm->pcm.sample_rate;
}

void mmm_pcm_set_format      (Mmm *fb, MmmAudioFormat format)
{
  fb->shm->pcm.format = format;
}

int mmm_pcm_bpf (Mmm *fb)
{
  MmmAudioFormat format = fb->shm->pcm.format;
  //return 2;
  switch (format)
  {
    case MMM_f32:  return 4;
    case MMM_f32S: return 8;
    case MMM_s16:  return 2;
    case MMM_s16S: return 4;
    default: return 1;
  }
}

static inline int mmm_pcm_frame_count (Mmm *fb)
{
  return (MMM_AUDIO_BUFFER_SIZE / mmm_pcm_bpf (fb));
}

int  mmm_pcm_get_queued_frames (Mmm *fb)
{
  int q = fb->shm->pcm.write;
  int p = fb->shm->pcm.read;

  if (p == q)
    /* |    p      | legend: .. played/unused */
    /* |....q......|         == queued        */
    return 0;
  
  else if (q > p)
    /* |  p        | */
    /* |..===q.....| */
    
    return (q - p);
  else
    /* |       p   | */
    /* |==q....====| */
    return q + (mmm_pcm_frame_count (fb) - p);
}

int  mmm_pcm_get_free_frames (Mmm *fb)
{
  int total = mmm_pcm_frame_count (fb);
  return total - mmm_pcm_get_queued_frames (fb) - 1;
}

int  mmm_pcm_get_frame_chunk (Mmm *fb)
{
  int total = mmm_pcm_frame_count (fb);
  int free = mmm_pcm_get_free_frames (fb);
  return free;
  int ret = total / 2;
  if (ret > free)
    ret = free;
  return ret;
}

//static long frame_no = 0;

int mmm_pcm_write (Mmm *fb, const int8_t *data, int frames)
{
  int total = mmm_pcm_frame_count (fb);
  uint8_t *seg1, *seg2 = NULL;
  int      seg1_len, seg2_len = 0;
  int      bpf = mmm_pcm_bpf (fb); /* bytes per frame */

  int q = fb->shm->pcm.write;
  int p = fb->shm->pcm.read;
  int ret = 0;

  if ((q == p - 1 ) || ((q == total-1) && p == 0))
  {
    fprintf (stderr, "%i frames audio overrun\n", frames);
    return ret;
  }
  if (p == q)
  {
    /* |     p     | legend: .. played/unused */
    /* |.....q.....|         == queued        */
    seg1     = &fb->shm->pcm.buffer[q * bpf];
    seg1_len = total - q;
    if (q < 2)
      seg1_len -= 2;
    else
    {
      seg2     = &fb->shm->pcm.buffer[0];
      seg2_len = q - 2;
    }
  }
  else if (q > p)
  {
    /* |  p        | */
    /* |..===q.....| */
    
    seg1     = &fb->shm->pcm.buffer[ q * bpf];
    seg1_len = total - q;
    seg2     = &fb->shm->pcm.buffer[0];
    seg2_len = p - 2;

  }
  else
  {
    /* |       p   | */
    /* |==q....====| */
    seg1 = &fb->shm->pcm.buffer[(q) * bpf];
    seg1_len = p-q;
    seg2_len = 0;
  }

  if (seg1_len > frames)
    seg1_len = frames;
  frames -= seg1_len;

  memcpy (seg1, data, seg1_len * bpf);
  data += seg1_len * bpf;
  ret += seg1_len;

#if USE_ATOMIC_OPS
  __sync_lock_test_and_set(&fb->shm->pcm.write,
                     (fb->shm->pcm.write + seg1_len) % total);
#else
  fb->shm->pcm.write = (fb->shm->pcm.queued_pos + seg1_len) % total;
#endif

  if (frames <= 0)
    return ret;
  if (seg2_len <= 0)
  {
    fprintf (stderr, "%i frames audio overrun\n", frames);
    return ret;
  }

  if (seg2_len > frames)
    seg2_len = frames;
  frames -= seg2_len;

  memcpy (seg2, data, seg2_len * bpf);
  ret += seg2_len;

#if USE_ATOMIC_OPS
  __sync_lock_test_and_set(&fb->shm->pcm.write,
                     (fb->shm->pcm.write + seg1_len) % total);
#else
  fb->shm->pcm.write = (fb->shm->pcm.queued_pos + seg1_len) % total;
#endif

  if (frames > 0)
    fprintf (stderr, "%i frames audio overrun\n", frames);

  return ret;
}

int  mmm_pcm_read (Mmm *fb, int8_t *data, int frames)
{
  int      total = mmm_pcm_frame_count (fb);
  uint8_t *seg1, *seg2 = NULL;
  int      seg1_len, seg2_len = 0;
  int      bpf = mmm_pcm_bpf (fb); /* bytes per frame */
  int      ret = 0;

  int q = fb->shm->pcm.write;
  int p = fb->shm->pcm.read;

  if (p == q)
  {
    /* |    p      | legend: .. played/unused */
    /* |....q......|         == queued        */
    return 0;
  }

  else if (q > p)
  {
    /* |  p        | */
    /* |..===q.....| */
    seg1     = &fb->shm->pcm.buffer[p * bpf];
    seg1_len = (q - p);
    seg2_len = 0;
  }
  else
  {
    seg1     = &fb->shm->pcm.buffer[p * bpf];
    seg1_len = total - p;
    seg2     = &fb->shm->pcm.buffer[0];
    seg2_len = q;
    /* |       p   | */
    /* |==q....====| */
  }

  if (seg1_len > frames)
    seg1_len = frames;
  frames -= seg1_len;

  memcpy (data, seg1, seg1_len * bpf);
  data += (seg1_len * bpf);
  ret += seg1_len;

#if USE_ATOMIC_OPS
  __sync_lock_test_and_set(&fb->shm->pcm.read,
                     (fb->shm->pcm.read + seg1_len) % total);
#else
  fb->shm->pcm.read = (fb->shm->pcm.read + seg1_len) % total;
#endif

  if (frames <= 0)
    return ret;
  if (seg2_len <= 0)
    return ret;

  if (seg2_len > frames)
    seg2_len = frames;
  frames -= seg2_len;

  memcpy (data, seg2, seg2_len * bpf);
  data += (seg2_len * bpf);
  ret += seg2_len;

#if USE_ATOMIC_OPS
  __sync_lock_test_and_set(&fb->shm->pcm.read,
                     (fb->shm->pcm.read + seg2_len) % total);
#else
  fb->shm->pcm.read = (fb->shm->pcm.read + seg2_len) % total;
#endif

  return ret;
}

/* message/event calls _can_ share more code */

int mmm_has_message (Mmm *fb)
{
  if (fb->shm->messages.read != fb->shm->messages.write)
    return 1;
  return 0;
}

void mmm_add_message (Mmm *fb, const char *message)
{
  MmmShm *shm = fb->shm;
  int message_no = shm->messages.write + 1;
  if (message_no >= MMM_MAX_EVENT)
    message_no = 0;

  if (message_no == shm->messages.read)
    {
      static int once = 0;
      if (!once)
        fprintf (stderr, "oc message queue overflow\n");
      once = 1;
      return;
    }

  strcpy ((void*)shm->messages.buffer[message_no], message);

  shm->messages.write ++;
  if (shm->messages.write >= MMM_MAX_EVENT)
    shm->messages.write = 0;
}

const char *mmm_get_message (Mmm *fb)
{
  if (fb->shm->messages.read != fb->shm->messages.write)
    {
      fb->shm->messages.read++;
      if (fb->shm->messages.read >= MMM_MAX_EVENT)
        fb->shm->messages.read = 0;
      return (void*)fb->shm->messages.buffer[fb->shm->messages.read];
    }
  return NULL;
}
