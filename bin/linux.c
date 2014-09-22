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

#include "mmm-evsource.h"

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

void linux_warp_cursor (Host *host, int x, int y)
{
  HostLinux *host_linux = (void*)host;
  int i;
  for (i = 0; i < host_linux->evsource_count; i++)
    evsource_set_coord (host_linux->evsource[i], x, y);
}

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
        if (host->client)
        {
          ufb_add_event (host->client->ufb, event);
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

static void render_client (Host *host, Client *client, float ptr_x, float ptr_y)
{
  HostLinux *host_linux = (void*)host;
  //SDL_Surface *screen = host_linux->screen;
  int width, height, rowstride;

  if (client->pid == getpid ())
    return;

  int cwidth, cheight;

  const unsigned char *pixels = ufb_get_buffer_read (client->ufb,
      &width, &height, &rowstride);
  int x, y;

  x = ufb_get_x (client->ufb);
  y = ufb_get_y (client->ufb);

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

    ufb_read_done (client->ufb);
  }

  ufb_host_get_size (client->ufb, &cwidth, &cheight);

#if 0
  if ( (cwidth  && cwidth  != host->width) ||
       (cheight && cheight != host->height))
  {
    fprintf (stderr, "%i, %i\n", cwidth, cheight);
    host_linux->screen = SDL_SetVideoMode (cwidth,
                                         cheight,32,
                                         SDL_SWSURFACE | SDL_RESIZABLE);
    host->width = cwidth;
    host->height = cheight;
    host->stride = host->width * host->bpp;
  }
#endif
}

#if 0

void host_destroy (Host *host)
{
  free (host);
}

static void mmfb_linux_fullscreen (Host *host, int fullscreen)
{
  HostLinux *host_linux = (void*)host;
  SDL_Surface *screen = host_linux->screen;
  int width = 640, height = 480;

  if (fullscreen)
  {
    SDL_Rect **modes;
    modes = SDL_ListModes(NULL, SDL_HWSURFACE|SDL_FULLSCREEN);
    if (modes == (SDL_Rect**)0) {
        fprintf(stderr, "No modes available!\n");
        return;
    }

    width = modes[0]->w;
    height = modes[0]->h;

    screen = SDL_SetVideoMode(width, height, 32,
                              SDL_SWSURFACE | SDL_FULLSCREEN );
    host_linux->screen = screen;
  }
  else
  {
    screen = SDL_SetVideoMode(width, height, 32,
                              SDL_SWSURFACE | SDL_RESIZABLE );
    host_linux->screen = screen;
  }
  host->width = width;
  host->bpp = 4;
  host->stride = host->width * host->bpp;
  host->height = height;
  host->fullscreen = fullscreen;
}

#endif

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

  host->width = host_linux->vinfo.xres;
  host->height = host_linux->vinfo.yres;

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

static int main_linux (const char *path)
{
  Host *host;

  setenv ("UFB_IS_COMPOSITOR", "foo", 1);
  host = host_linux_new (path, 640, 480);
  HostLinux *host_linux = (void*)host;
  host_linux = (void*) host;
  unsetenv ("UFB_IS_COMPOSITOR");

  //atexit (SDL_Quit);

  while (!host_has_quit)
  {
    if (!event_check_pending (host))
      usleep (10000);

    host_idle_check (host);
    host_monitor_dir (host);

    if (host->client)
      render_client (host, host->client, 0, 0);

    host_clear_dirt (host);
  }
  return 0;
}

int main (int argc, char **argv)
{
  const char *path = "/tmp/ufb";
  setenv ("UFB_PATH", path, 1);

  if (argv[1] == NULL)
    return main_linux (path);

  if (argv[1])
    switch (fork())
    {
      case 0:
        return main_linux (path);
      case -1:
        fprintf (stderr, "fork failed\n");
        return 0;
    }
  execvp (argv[1], argv+1);
  return 0;
}
