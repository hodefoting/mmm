

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mmfb.h"

#define UFB_WAIT_ATTEMPTS 30
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

#define UFB_MAX_EVENT  1024

typedef enum {
  UFB_INITIALIZING = 0,
  UFB_NEUTRAL,
  UFB_DRAWING,
  UFB_WAIT_FLIP,
  UFB_FLIPPING,
} UfbFlipState;

typedef struct _UfbShm UfbShm;

#define UFB_AUDIO_BUFFER_SIZE  4096

static char *UFB_magic    = "UFB ░ ";
static char *UFB_fb       = "FB      ";
static char *UFB_events   = "EVENTS  ";
static char *UFB_messages = "MESSAGES";
static char *UFB_pcm      = "PCM     ";
static char *UFB_fbdata   = "FBDATA  ";

typedef struct UfbBlock {
  uint64_t type;
  uint32_t length;
  uint32_t next;
} UfbBlock;

/* In the comments, a C means the client (generally) writes and a H means the
 * host (generally) writes,
 */

typedef struct UfbHeader {
 UfbBlock   block;

 uint32_t   client_version;  /* */
 uint32_t   server_version;  /* */
 int32_t    pid;             /* C (by _convention_ 64bit systems also have 32bit pids)*/
 int        lock;            /* */
 /* revision?                   */
 /* flags.. */
 uint32_t   pad[32];
} UfbHeader;

typedef struct UfbFb {
 UfbBlock   block;
 uint8_t    title[256];      /* C  window title  */ /* XXX: need generic properties?  */
 uint8_t   *babl_format[128];/* C  pixel format; according to babls conventions */
 int32_t    width;           /* C  width of raster in pixels */
 int32_t    height;          /* C  height of raster in pixels */
 int32_t    stride;          /* C  byte offset between starts of */
                                   /* pixelrows in memory   */
 int32_t    fb_offset;       /* offset where fb is located */

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
} UfbFb;

typedef struct UfbEvents {
  UfbBlock       block;
  int16_t        read;      /* C  last event_no which has been read    */
  int16_t        write;     /* H  last event_no which has been written */
  uint32_t       pad[8];
  uint8_t        buffer[UFB_MAX_EVENT][128]; /* S circular list of events    */
} UfbEvents;

typedef struct UfbMessages {
  UfbBlock       block;
  int16_t        read;      /* H  last message_no which has been read    */
  int16_t        write;     /* C  last message_no which has been written */
  uint32_t       pad[8];
  uint8_t        buffer[UFB_MAX_EVENT][128]; /* C circular list of events    */
} UfbMessages;

typedef struct UfbPcm {
  UfbBlock       block;
  UfbAudioFormat format;                        /* C */
  int            sample_rate;                   /* C */ 
  int            host_sample_rate;              /* H */
  int            read;                          /* H */
  int            write;                         /* C */
  uint32_t       pad[16];
  uint8_t        buffer[UFB_AUDIO_BUFFER_SIZE]; /* C */
} UfbPcm;

struct  _UfbShm { 
  UfbHeader      header;    /* must be first  in file */
  UfbFb          fb;        /* must be second in file */
  UfbEvents      events;    /* must be third  in file */
  UfbMessages    messages;  /* must be fourth in file */
  UfbPcm         pcm;       /* must be fifth  in file */

  /* ... potential new blocks  ... */

  UfbBlock       pixeldata; /* offset for pixeldata is defined in fb */
};

static Ufb *ufb_new_shm (int width, int height, void *babl_format);

struct Ufb_
{
  int          bpp;      /* do not move, inline function in header ..*/
  int          stride;   /* .. depends on position of bpp/stride */

  uint8_t     *fb;   /* pointer to actual pixels */
  int          width;
  int          height;
  int          mapped_size;

  void        *format;  /* babl format */
                        /* XXX: add format for frontbuffer*/

  char        *path;
  int          fd;

  UfbShm      *shm;

  int          compositor_side;
};

static void ufb_remap (Ufb *fb);

int ufb_get_bytes_per_pixel (Ufb *fb)
{
  return fb->bpp;
}

static struct timeval start_time;
#define usecs(time) ((time.tv_sec - start_time.tv_sec) * 1000000 + time.tv_usec)

static void ufb_init_ticks (void)
{
  static int done = 0;

  if (done)
    return;
  done = 1;
  gettimeofday (&start_time, NULL);
}

inline static long ticks (void)
{
  struct timeval measure_time;
  ufb_init_ticks ();
  gettimeofday (&measure_time, NULL);
  return usecs (measure_time) - usecs (start_time);
}

#undef usecs

long ufb_ticks (void)
{
  return ticks ();
}

int
ufb_wait_neutral (Ufb *fb)
{
  int attempts = UFB_WAIT_ATTEMPTS;

  while (fb->shm->fb.flip_state != UFB_NEUTRAL && --attempts)
    usleep (500);

  return (attempts > 0 ? 0 : -1);
} 

static int
ufb_set_state (Ufb *fb, UfbFlipState state)
{
  fb->shm->fb.flip_state = state;
  return 1;
}

unsigned char *
ufb_get_buffer_write (Ufb *fb, int *width, int *height, int *stride,
    void *babl_format)
{
  ufb_wait_neutral (fb);
  //fprintf (stderr, "[%i]", fb->bpp);

  ufb_set_state (fb, UFB_DRAWING);
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

void _ufb_get_coords (Ufb *ufb, double *x, double *y);

void
ufb_write_done (Ufb *fb, int x, int y, int width, int height)
{
  if (width == 0 && height == 0)
    {
      /* nothing written */
      fb->shm->fb.flip_state = UFB_NEUTRAL;
      return;
    }
  fb->shm->fb.flip_state = UFB_WAIT_FLIP;

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

  fb->shm->fb.flip_state = UFB_WAIT_FLIP;
}

int
ufb_wait_neutral_or_wait_flip (Ufb *fb)
{
  int attempts = UFB_WAIT_ATTEMPTS;

  while (fb->shm->fb.flip_state != UFB_NEUTRAL &&
         fb->shm->fb.flip_state != UFB_WAIT_FLIP &&
         --attempts)
    usleep (500);

  if (attempts < 1)
    fprintf (stderr, "eeew\n");

  return   (attempts > 0 ? 0 : -1 );
}

const unsigned char *
ufb_get_buffer_read (Ufb *fb, int *width, int *height, int *stride)
{
  if (width)  *width  = fb->width;
  if (height) *height = fb->height;

  if(ufb_host_check_size (fb, NULL, NULL))
    return NULL;
  if (ufb_wait_neutral_or_wait_flip (fb))
    return NULL;

  if (stride) *stride = fb->stride;

  ufb_set_state (fb, UFB_FLIPPING);

  return (void*)fb->fb; /* the direct device XXX: should use double buffering for toplevel */
}

void
ufb_read_done (Ufb *fb)
{
  fb->shm->fb.damage_x = 0;
  fb->shm->fb.damage_y = 0;
  fb->shm->fb.damage_width = 0;
  fb->shm->fb.damage_height = 0;
  fb->shm->fb.flip_state = UFB_NEUTRAL;
}

Ufb *
ufb_host_open (const char *path)
{
  Ufb *fb = calloc (sizeof (Ufb), 1);

  fb->compositor_side = 1;
  fb->fd = open (path, O_RDWR);
  if (fb->fd == -1)
    {
      free (fb);
      return NULL;
    }
  
  /* first we map just the UfbShm struct */
  fb->shm = mmap (NULL, sizeof (UfbShm), PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
  fb->mapped_size = sizeof (UfbShm);
  fb->bpp = 4;
  fb->width = fb->shm->fb.width;
  fb->height = fb->shm->fb.height;
  fb->stride = fb->shm->fb.width * fb->bpp;

  ufb_remap (fb);
  return fb;
}
static void ufb_init_header (UfbShm *shm);

static void
ufb_remap (Ufb *fb)
{
  {
    int size = sizeof(UfbShm) + fb->shm->fb.height * fb->shm->fb.stride;
    if (size > fb->mapped_size)
      {
        if (pwrite (fb->fd, "", 1, size+1) == -1)
          fprintf (stderr, "ufb failed stretching\n");
        fsync (fb->fd);
        fb->shm = mremap (fb->shm, fb->mapped_size, size, MREMAP_MAYMOVE);
        if (!fb->shm)
          fprintf (stderr, "eeeek!\n");
        fb->mapped_size = size;
      }
    if (!fb->compositor_side)
      ufb_init_header (fb->shm);
  }

  fb->width  = fb->shm->fb.width;
  fb->height = fb->shm->fb.height;
  fb->stride = fb->shm->fb.stride = fb->width * fb->bpp;
  fb->fb = ((uint8_t*)fb->shm) + fb->shm->fb.fb_offset;
}

int ufb_get_width  (Ufb *fb)
{
  return fb->width;
}
int ufb_get_height (Ufb *fb)
{
  return fb->height;
}

void
ufb_get_size (Ufb *fb, int *width, int *height)
{
  if (width)
    *width = fb->width;
  if (height)
    *height = fb->height;
}

int
ufb_host_check_size (Ufb *fb, int *width, int *height)
{
  int ret = 0;
  if (fb->width != fb->shm->fb.width ||
      fb->height != fb->shm->fb.height)
    {
      ufb_remap (fb);
      ret = 1;
      fb->stride = fb->shm->fb.stride;
      fb->width = fb->shm->fb.width;
    }
  if (width || height)
    ufb_get_size (fb, width, height);
  return ret;
}

int
ufb_client_check_size (Ufb *fb, int *width, int *height)
{
  int ret = 0;
  if (fb->shm->fb.desired_width  != fb->shm->fb.width ||
      fb->shm->fb.desired_height != fb->shm->fb.height)
    {
      ufb_set_size (fb, fb->shm->fb.desired_width, fb->shm->fb.desired_height);
      ret = 1;
    }
  if (width || height)
    ufb_get_size (fb, width, height);
  return ret;
}

void ufb_set_size (Ufb *fb, int width, int height)
{
  while ((fb->shm->fb.flip_state != UFB_NEUTRAL) &&
         (fb->shm->fb.flip_state != UFB_INITIALIZING))
    usleep (500);
  fb->shm->fb.flip_state = UFB_INITIALIZING;

  fb->shm->fb.width = fb->shm->fb.desired_width =  width;
  fb->shm->fb.height = fb->shm->fb.desired_height = height;
  fb->shm->fb.stride = fb->shm->fb.width * fb->bpp;
  ufb_remap (fb);
  fb->shm->fb.flip_state = UFB_NEUTRAL;
}

int ufb_has_event (Ufb *fb)
{
  if (fb->shm->events.read != fb->shm->events.write)
    return 1;
  return 0;
}

void ufb_add_event (Ufb *fb, const char *event)
{
  UfbShm *shm = fb->shm;
  int event_no = shm->events.write + 1;
  if (event_no >= UFB_MAX_EVENT)
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
  if (shm->events.write >= UFB_MAX_EVENT)
    shm->events.write = 0;
}


const char *ufb_get_event (Ufb *fb)
{
  if (fb->shm->events.read != fb->shm->events.write)
    {
      fb->shm->events.read++;
      if (fb->shm->events.read >= UFB_MAX_EVENT)
        fb->shm->events.read = 0;
      return (void*)fb->shm->events.buffer[fb->shm->events.read];
    }
  return NULL;
}

Ufb *ufb_new (int width, int height, UfbFlag flags, void *babl_format)
{
  Ufb *fb = NULL;

  if (!getenv ("UFB_PATH"))
  {
    switch (fork())
    {
      case 0: /* child */
        execlp ("mmfb", "mmfb", NULL);
      case -1:
        fprintf (stderr, "fork failed\n");
        return 0;
    }
    setenv ("UFB_PATH", "/tmp/ufb", 1);
  }

  {
    int is_compositor = (getenv ("UFB_COMPOSITOR") != NULL);
    const char *env = getenv ("UFB_PATH");
    if (env && !is_compositor)
    {
      fb = ufb_new_shm (width, height, babl_format);
      ufb_set_size (fb, width, height);
    }
  }

  if (!fb)
  { 
    fprintf (stderr, "failed to initialize framebuffer\n");
    exit (-1);
  }

  if (fb)
    {
      ufb_set_title (fb, "ufb");
    }

  if (!fb)
    {
      fprintf (stderr, "unable to get framebuffer\n");
    }

  ufb_pcm_set_sample_rate (fb, 44100);
  ufb_pcm_set_format (fb, UFB_s16);

  return fb;
}

#include <assert.h>

static void ufb_init_header (UfbShm *shm)
{
  int pos = 0;
  int length;

  shm->fb.fb_offset = sizeof (UfbShm);
  length = sizeof (UfbHeader);
  shm->header.block.length = length;
  pos += length;
  shm->header.block.next = pos;
  assert (strlen (UFB_magic) == 8);
  memcpy (&shm->header.block.type, UFB_magic, 8);

  length = sizeof(UfbFb);
  shm->fb.block.length = length;
  pos += length;
  shm->fb.block.next = pos;
  assert (strlen (UFB_fb) == 8);
  memcpy (&shm->fb.block.type, UFB_fb, 8);

  length = sizeof (UfbEvents);
  shm->events.block.length = length;
  pos += length;
  shm->events.block.next = pos;
  assert (strlen (UFB_events) == 8);
  memcpy (&shm->events.block.type, UFB_events, 8);

  length = sizeof (UfbMessages);
  shm->messages.block.length = length;
  pos += length;
  shm->messages.block.next = pos;
  assert (strlen (UFB_messages) == 8);
  memcpy (&shm->messages.block.type, UFB_messages, 8);

  length = sizeof (UfbPcm);
  shm->pcm.block.length = length;
  pos += length;
  shm->pcm.block.next = pos;
  assert (strlen (UFB_pcm) == 8);
  memcpy (&shm->pcm.block.type, UFB_pcm, 8);

  assert (strlen (UFB_fbdata) == 8);
  memcpy (&shm->pixeldata.type, UFB_fbdata, 8);
}

static Ufb *ufb_new_shm (int width, int height, void *babl_format)
{
  Ufb *fb = calloc (sizeof (Ufb), 1);
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
    const char *ufb_path = getenv("UFB_PATH");
    if (!ufb_path) ufb_path = "/tmp";
    sprintf (buf, "%s/fb.XXXXXX", ufb_path);
    fb->path = strdup (buf);
  }
  fprintf (stderr, "%s\n", fb->path);
  fb->fd = mkstemp (fb->path);
  pwrite (fb->fd, "", 1, sizeof (UfbShm) + fb->stride * fb->height);
  fsync (fb->fd);

  chmod (fb->path, 511);

  fb->mapped_size = fb->stride * fb->height + sizeof (UfbShm);
  fb->shm = mmap (NULL, fb->mapped_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
  ufb_init_header (fb->shm);

  fb->shm->fb.desired_width = fb->width;
  fb->shm->fb.desired_height = fb->height;
  fb->shm->fb.width = fb->width;
  fb->shm->fb.stride = fb->stride;
  fb->shm->fb.height = fb->height;
  fb->shm->fb.flip_state = UFB_NEUTRAL;
  fb->shm->header.pid = getpid ();
  fprintf (stderr, "pidset\n");
  ufb_remap (fb);

  return fb;
}

void
ufb_destroy (Ufb *fb)
{
  munmap (fb->shm, fb->mapped_size);
  if (fb->fd)
    close (fb->fd);
  free (fb);
}

int ufb_get_damage (Ufb *fb, int *x, int *y, int *width, int *height)
{
  if (x)
    *x = fb->shm->fb.damage_x;
  if (y)
    *y = fb->shm->fb.damage_y;
  if (width)
    *width = fb->shm->fb.damage_width;
  if (height)
    *height = fb->shm->fb.damage_height;
  return fb->shm->fb.flip_state == UFB_WAIT_FLIP;
}

const char *ufb_get_babl_format (Ufb *fb)
{
  return (char *)fb->shm->fb.babl_format;
}

void
ufb_set_title (Ufb *ufb, const char *title)
{
  strncpy ((void*)ufb->shm->fb.title, title, 512);
}

const char *
ufb_get_title (Ufb *ufb)
{
  return (void*)ufb->shm->fb.title;
}

void ufb_set_x (Ufb *fb, int x)
{
  fb->shm->fb.x = x;
}

void ufb_set_y (Ufb *fb, int y)
{
  fb->shm->fb.y = y;
}

int  ufb_get_x (Ufb *fb)
{
  return fb->shm->fb.x;
}

int ufb_get_y (Ufb *fb)
{
  return fb->shm->fb.y;
}

long ufb_client_pid (Ufb *fb)
{
  return fb->shm->header.pid;
}

void ufb_host_get_size (Ufb *fb, int *width, int *height)
{
  if (!fb)
    return;
  if (width)
    *width = fb->shm->fb.desired_width;
  if (height)
    *height = fb->shm->fb.desired_height;
}

void ufb_host_set_size (Ufb *fb, int width, int height)
{
  if (!fb)
    return;
  fb->shm->fb.desired_width = width;
  fb->shm->fb.desired_height = height;
}

void ufb_pcm_set_sample_rate (Ufb *fb, int freq)
{
  fb->shm->pcm.sample_rate = freq;
}

int ufb_pcm_get_sample_rate (Ufb *fb)
{
  return fb->shm->pcm.sample_rate;
}

void ufb_pcm_set_format      (Ufb *fb, UfbAudioFormat format)
{
  fb->shm->pcm.format = format;
}

int ufb_pcm_bpf (Ufb *fb)
{
  UfbAudioFormat format = fb->shm->pcm.format;
  //return 2;
  switch (format)
  {
    case UFB_f32:  return 4;
    case UFB_f32S: return 8;
    case UFB_s16:  return 2;
    case UFB_s16S: return 4;
    default: return 1;
  }
}

static inline int ufb_pcm_frame_count (Ufb *fb)
{
  return (UFB_AUDIO_BUFFER_SIZE / ufb_pcm_bpf (fb));
}

int  ufb_pcm_get_queued_frames (Ufb *fb)
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
    return q + (ufb_pcm_frame_count (fb) - p);
}

int  ufb_pcm_get_free_frames (Ufb *fb)
{
  int total = ufb_pcm_frame_count (fb);
  return total - ufb_pcm_get_queued_frames (fb) - 1;
}

int  ufb_pcm_get_frame_chunk (Ufb *fb)
{
  int total = ufb_pcm_frame_count (fb);
  int free = ufb_pcm_get_free_frames (fb);
  return free;
  int ret = total / 2;
  if (ret > free)
    ret = free;
  return ret;
}

//static long frame_no = 0;

int ufb_pcm_write (Ufb *fb, const int8_t *data, int frames)
{
  int total = ufb_pcm_frame_count (fb);
  uint8_t *seg1, *seg2 = NULL;
  int      seg1_len, seg2_len = 0;
  int      bpf = ufb_pcm_bpf (fb); /* bytes per frame */

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

int  ufb_pcm_read (Ufb *fb, int8_t *data, int frames)
{
  int      total = ufb_pcm_frame_count (fb);
  uint8_t *seg1, *seg2 = NULL;
  int      seg1_len, seg2_len = 0;
  int      bpf = ufb_pcm_bpf (fb); /* bytes per frame */
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

int ufb_has_message (Ufb *fb)
{
  if (fb->shm->messages.read != fb->shm->messages.write)
    return 1;
  return 0;
}

void ufb_add_message (Ufb *fb, const char *message)
{
  UfbShm *shm = fb->shm;
  int message_no = shm->messages.write + 1;
  if (message_no >= UFB_MAX_EVENT)
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
  if (shm->messages.write >= UFB_MAX_EVENT)
    shm->messages.write = 0;
}

const char *ufb_get_message (Ufb *fb)
{
  if (fb->shm->messages.read != fb->shm->messages.write)
    {
      fb->shm->messages.read++;
      if (fb->shm->messages.read >= UFB_MAX_EVENT)
        fb->shm->messages.read = 0;
      return (void*)fb->shm->messages.buffer[fb->shm->messages.read];
    }
  return NULL;
}

#if 0
static void flip (Ufb *fb)
{
  if (fb->fb_width == fb->width &&
      fb->fb_height == fb->height &&
      fb->fb_stride == fb->stride)
  {
    /* fast path for full buffer copy */
    memcpy (fb->front_buffer, fb->fb, fb->height * fb->stride);
    return;
  }

  switch (fb->fb_bpp)
  {
    case 2:
#if 0
    {
      int front_offset = 0;
      char *dst = fb->front_buffer + front_offset;
      char *src = fb->fb;

      for (int scan = 0; scan < fb->height; scan++)
      {
        memcpy32_16 (dst, src, fb->width);

        dst += fb->fb_stride;
        src += fb->stride;
      }
    }
#else
    {
      int front_offset = fb->shm->fb.y * fb->fb_stride + fb->shm->fb.x * fb->fb_bpp;
      uint8_t *dst = fb->front_buffer + front_offset;
      uint8_t *src = fb->fb;
      int copystride = fb->fb_stride - fb->shm->fb.x * fb->fb_bpp;
      if (copystride > fb->width * fb->fb_bpp)
        copystride = fb->width * fb->fb_bpp;
      copystride /= fb->fb_bpp;
      for (int scan = 0; scan < fb->height; scan++)
      {
        if (dst >= fb->front_buffer &&
            dst <  fb->front_buffer + fb->fb_stride * (fb->fb_height) - copystride)
        {
          memcpy32_16 (dst, src, copystride);
        }
        dst += fb->fb_stride;
        src += fb->stride;
      }
#if 0
      /* draw cursor */
      for (int u = -2; u < 2; u++)
      for (int v = -2; v < 2; v++)
      {
        double cx, cy;
        _ufb_get_coords (fb, &cx, &cy);
        cx += u;
        cy += v;
        front_offset = cy * fb->fb_stride + cx * fb->fb_bpp;
        dst = fb->front_buffer + front_offset;
        if (dst >= fb->front_buffer &&
            dst <  fb->front_buffer + fb->fb_stride * (fb->fb_height) - fb->fb_bpp &&
            cx < fb->fb_width &&
            cx > 0)
        {
          memset (dst, 255, fb->fb_bpp);
        }
      }
      for (int u = -1; u < 1; u++)
      for (int v = -1; v < 1; v++)
      {
        double cx, cy;
        _ufb_get_coords (fb, &cx, &cy);
        cx += u;
        cy += v;
        front_offset = cy * fb->fb_stride + cx * fb->fb_bpp;
        dst = fb->front_buffer + front_offset;
        if (dst >= fb->front_buffer &&
            dst <  fb->front_buffer + fb->fb_stride * (fb->fb_height) - fb->fb_bpp &&
            cx < fb->fb_width &&
            cx > 0)
        {
          memset (dst, 0, fb->fb_bpp);
        }
      }
#endif
    }

#endif

    break;
    case 4:
    {
      int front_offset = fb->shm->fb.y * fb->fb_stride + fb->shm->fb.x * fb->fb_bpp;
      uint8_t *dst = fb->front_buffer + front_offset;
      uint8_t *src = fb->fb;
      int copystride = fb->fb_stride - fb->shm->fb.x * fb->fb_bpp;
      if (copystride > fb->width * fb->fb_bpp)
        copystride = fb->width * fb->fb_bpp;
      for (int scan = 0; scan < fb->height; scan++)
      {
        if (dst >= fb->front_buffer &&
            dst <  fb->front_buffer + fb->fb_stride * (fb->fb_height) - copystride)
        {
          memcpy (dst, src, copystride);
        }
        dst += fb->fb_stride;
        src += fb->stride;
      }
#if 0
      /* draw cursor */
      for (int u = -2; u < 2; u++)
      for (int v = -2; v < 2; v++)
      {
        double cx, cy;
        _ufb_get_coords (fb, &cx, &cy);
        cx += u;
        cy += v;
        front_offset = cy * fb->fb_stride + cx * fb->fb_bpp;
        dst = fb->front_buffer + front_offset;
        if (dst >= fb->front_buffer &&
            dst <  fb->front_buffer + fb->fb_stride * (fb->fb_height) - fb->fb_bpp &&
            cx < fb->fb_width &&
            cx > 0)
        {
          memset (dst, 255, fb->fb_bpp);
        }
      }
      for (int u = -1; u < 1; u++)
      for (int v = -1; v < 1; v++)
      {
        double cx, cy;
        _ufb_get_coords (fb, &cx, &cy);
        cx += u;
        cy += v;
        front_offset = cy * fb->fb_stride + cx * fb->fb_bpp;
        dst = fb->front_buffer + front_offset;
        if (dst >= fb->front_buffer &&
            dst <  fb->front_buffer + fb->fb_stride * (fb->fb_height) - fb->fb_bpp &&
            cx < fb->fb_width &&
            cx > 0)
        {
          memset (dst, 0, fb->fb_bpp);
        }
      }
#endif
    }
    break;
  }
}
#endif
