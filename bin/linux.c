#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "mmm.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdint.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "host.h"

#include "linux-evsource.h"

EvSource *evsource_ts_new (void);
EvSource *evsource_kb_new (void);
EvSource *evsource_mice_new (void);

typedef struct _HostLinux   HostLinux;

/////////////////////////////////////////////////////////////////////

struct _HostLinux
{
  Host         host;
  char        *path;
  int          fb_fd;
  int          fb_bpp;
  uint8_t     *front_buffer;
  int          fb_stride;
  int          fb_width;
  int          fb_height;
  int          fb_mapped_size;
  struct       fb_var_screeninfo vinfo;
  struct       fb_fix_screeninfo finfo;

  EvSource    *evsource[4];
  int          evsource_count;
};

EvSource *evsource_ts_new (void);
EvSource *evsource_kb_new (void);
EvSource *evsource_mice_new (void);

static int host_add_evsource (Host *host, EvSource *source)
{
  HostLinux *host_linux = (void*)host;
  if (source)
  {
    host_linux->evsource[host_linux->evsource_count++] = source;
  }
}

static int event_check_pending (Host *host)
{
  HostLinux *host_linux = (void*)host;
  int i;
  int had_event = 0;
  for (i = 0; i < host_linux->evsource_count; i++)
  {
    while (evsource_has_event (host_linux->evsource[i]))
    {
      char *event = evsource_get_event (host_linux->evsource[i]);
      if (event)
      {
        if (host->focused)
        {
          mmm_add_event (host->focused->mmm, event);
          free (event);
          had_event ++;
        }
      }
    }
  }

  return had_event != 0;
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

#if 0
static uint8_t cursor[16][16]={
{1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,2,1,1,0,0,0,0,0,0,0,0,0,0,0,0},
{0,1,2,2,1,1,0,0,0,0,0,0,0,0,0,0},
{0,1,2,2,2,2,1,1,0,0,0,0,0,0,0,0},
{0,0,1,2,2,2,2,2,1,1,0,0,0,0,0,0},
{0,0,1,2,2,2,2,2,2,2,1,1,0,0,0,0},
{0,0,0,1,2,2,2,2,2,2,2,2,1,1,0,0},
{0,0,0,1,2,2,2,2,1,1,1,1,1,1,1,1},
{0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0},
{0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0},
{0,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0},
{0,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0}};

#else

static uint8_t cursor[16][16]={
{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
{1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
{1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
{1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0},
{1,2,2,2,2,2,2,2,1,1,0,0,0,0,0,0},
{1,2,2,2,2,2,1,1,0,0,0,0,0,0,0,0},
{1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
{1,2,1,1,2,2,1,0,0,0,0,0,0,0,0,0},
{1,1,0,1,2,2,2,1,0,0,0,0,0,0,0,0},
{0,0,0,0,1,2,2,1,0,0,0,0,0,0,0,0},
{0,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0}};

#endif


static void render_client (Host *host, Client *client, float ptr_x, float ptr_y)
{
  HostLinux *host_linux = (void*)host;
  //SDL_Surface *screen = host_linux->screen;
  int width, height, rowstride;

  if (client->pid == getpid ())
    return;

  int cwidth, cheight;

  const unsigned char *pixels = mmm_get_buffer_read (client->mmm,
      &width, &height, &rowstride);
  int x, y;

  x = mmm_get_x (client->mmm);
  y = mmm_get_y (client->mmm);

  if (ptr_x >= x && ptr_x < x + width &&
      ptr_y >= y && ptr_y < y + height)
    {
      host->focused = client;
    }

  if (pixels && width && height)
  {
    int front_offset = y * host->stride + x * host->bpp;
    uint8_t *dst = host_linux->front_buffer + front_offset;
    const uint8_t *src = pixels;
    int copystride = host->stride - x * host->bpp;
    int scan;
    if (copystride > width * host->bpp)
    {
      copystride = width * host->bpp;
    }
    for (scan = 0; scan < height; scan ++)
    {
      if (dst >= (uint8_t*)host_linux->front_buffer &&
          dst < ((uint8_t*)host_linux->front_buffer) + (host_linux->fb_stride * host->height) - copystride)
      {
        if (host->bpp == 4)
          memcpy (dst, src, copystride);
        else
          memcpy32_16 (dst, src, copystride / host->bpp);
      }
      dst += host_linux->fb_stride;
      src += rowstride;
    }
    mmm_read_done (client->mmm);
  }


  /* drawing of the cursor should be separated from the blitting
   * maybe even copying from the frontbuffer the contents it
   * is overdrawing; (and undrawing the cursor before each flip)
   */

  { /* draw cursor */
    int u,v;
    for (v = 0; v < 16; v ++)
      if (v + ptr_y > 0 && v + ptr_y < host->height)
        for (u = 0; u < 16; u ++)
        {
          if (u + ptr_x > 0 && u + ptr_x < host->width)
          {
            int o = host_linux->fb_stride * ((int)ptr_y+v) + host->bpp * ((int)(ptr_x)+u);
            if (cursor[v][u] == 1)
            {
              host_linux->front_buffer[o+0] = 255;
              host_linux->front_buffer[o+1] = 255;
              if (host->bpp == 4)
              {
                host_linux->front_buffer[o+2] = 255;
                host_linux->front_buffer[o+3] = 255;
              }
            }
            else if (cursor[v][u] == 2)
            {
              host_linux->front_buffer[o+0] = 0;
              host_linux->front_buffer[o+1] = 0;
              if (host->bpp == 4)
              {
                host_linux->front_buffer[o+2] = 0;
                host_linux->front_buffer[o+3] = 255;
              }
            }
          }
        }
  }

  mmm_host_get_size (client->mmm, &cwidth, &cheight);
}

Host *host_linux_new (const char *path, int width, int height)
{
  Host *host = calloc (sizeof (HostLinux), 1);
  HostLinux *host_linux = (void*)host;
  host->fbdir = strdup (path);

  if (getenv ("DISPLAY"))
  {
    fprintf (stderr, "Abort, tried to initialize linux fb from X\n");
    exit (-1);
  }

  if (width < 0)
  {
    width = 640;
    height = 480;
    host->fullscreen = 1;
  }

  host->width = width;
  host->bpp = 4;
  host->stride = host->width * host->bpp;
  host->height = height;

  host_linux->fb_fd = open ("/dev/fb0", O_RDWR);
  if (host_linux->fb_fd > 0)
    host_linux->path = strdup ("/dev/fb0");
  else
  {
    host_linux->fb_fd = open ("/dev/graphics/fb0", O_RDWR);
    if (host_linux->fb_fd > 0)
    {
      host_linux->path = strdup ("/dev/graphics/fb0");
    }
    else
    {
      free (host_linux);
      return NULL;
    }
  }

  if (ioctl(host_linux->fb_fd, FBIOGET_FSCREENINFO, &host_linux->finfo))
    {
      fprintf (stderr, "error getting fbinfo\n");
      close (host_linux->fb_fd);
      free (host_linux->path);
      free (host_linux);
      return NULL;
    }

   if (ioctl(host_linux->fb_fd, FBIOGET_VSCREENINFO, &host_linux->vinfo))
     {
       fprintf (stderr, "error getting fbinfo\n");
       close (host_linux->fb_fd);
       free (host_linux->path);
       free (host_linux);
       return NULL;
     }

  host_linux->fb_bpp = host_linux->vinfo.bits_per_pixel / 8;
  host_linux->fb_stride = host_linux->finfo.line_length;
  host_linux->fb_mapped_size = host_linux->finfo.smem_len;
  host_linux->front_buffer = mmap (NULL, host_linux->fb_mapped_size, PROT_READ|PROT_WRITE, MAP_SHARED, host_linux->fb_fd, 0);
  memset (host_linux->front_buffer, 255, host_linux->fb_mapped_size);

  if (host->fullscreen)
  {
    host->width = host_linux->vinfo.xres;
    host->stride = host->width * host->bpp;
    host->height = host_linux->vinfo.yres;
  }

  host_clear_dirt (host);

  host_add_evsource (host, evsource_kb_new ());
  host_add_evsource (host, evsource_mice_new ());

  return host;
}

static int
mmfb_unichar_to_utf8 (unsigned int  ch,
                      unsigned char*dest)
{
/* http://www.cprogramming.com/tutorial/utf8.c  */
/*  Basic UTF-8 manipulation routines
  by Jeff Bezanson
  placed in the public domain Fall 2005 ... */
    if (ch < 0x80) {
        dest[0] = (char)ch;
        return 1;
    }
    if (ch < 0x800) {
        dest[0] = (ch>>6) | 0xC0;
        dest[1] = (ch & 0x3F) | 0x80;
        return 2;
    }
    if (ch < 0x10000) {
        dest[0] = (ch>>12) | 0xE0;
        dest[1] = ((ch>>6) & 0x3F) | 0x80;
        dest[2] = (ch & 0x3F) | 0x80;
        return 3;
    }
    if (ch < 0x110000) {
        dest[0] = (ch>>18) | 0xF0;
        dest[1] = ((ch>>12) & 0x3F) | 0x80;
        dest[2] = ((ch>>6) & 0x3F) | 0x80;
        dest[3] = (ch & 0x3F) | 0x80;
        return 4;
    }
    return 0;
}

void linux_warp_cursor (Host *host, int x, int y)
{
  HostLinux *host_linux = (void*)host;
  int i;
  for (i = 0; i < host_linux->evsource_count; i++)
    evsource_set_coord (host_linux->evsource[i], x, y);
}

static int main_linux (const char *path, int single)
{
  Host *host;

  host = host_linux_new (path, -1, -1);
  HostLinux *host_linux = (void*)host;
  host_linux = (void*) host;

  host->single_app = single;

  while (!host_has_quit)
  {
    int got_event;

    got_event = event_check_pending (host);
    host_idle_check (host);
    host_monitor_dir (host);

    if (got_event || host_is_dirty (host))
    {
      int warp = 0;
      double px, py;
      MmmList *l;

      if (!host->single_app)
        host->focused = NULL;

      _mmm_get_coords (NULL, &px, &py);
      if (px < 0) { px = 0; warp = 1; }
      if (py < 0) { py = 0; warp = 1; }
      if (py > host->height) { py = host->height; warp = 1; }
      if (px > host->width) { px = host->width; warp = 1; }
      if (warp)
      {
        linux_warp_cursor (host, px, py);
      }

      for (l = host->clients; l; l = l->next)
      {
        render_client (host, l->data, px, py);
      }
      host_clear_dirt (host);
    }
    else
    {
      usleep (10000);
    }

  }
  return 0;
}

int main (int argc, char **argv)
{
  char path[256];
  
  if (argv[1] == NULL)
  {
    if (!getenv ("MMM_PATH"))
    {
      sprintf (path, "/tmp/mmm-%i", getpid());
      setenv ("MMM_PATH", path, 1);
      mkdir (path, 0777);
    }
    return main_linux (path, 0);
  }

  if (argv[1][0] == '-' &&
      argv[1][1] == 'p' &&
      argv[2])
  {
    return main_linux (argv[2], 1);
  }

  if (!getenv ("MMM_PATH"))
  {
    sprintf (path, "/tmp/mmm-%i", getpid());
    setenv ("MMM_PATH", path, 1);
    mkdir (path, 0777);
  }

  if (argv[1])
    switch (fork())
    {
      case 0:
        return main_linux (path, 1);
      case -1:
        fprintf (stderr, "fork failed\n");
        return 0;
    }
  execvp (argv[1], argv+1);
  return 0;
}
