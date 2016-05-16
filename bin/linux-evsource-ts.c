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

#include <fcntl.h>
#include <linux/input.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "linux-evsource.h"

/* written to work with the zforce ir touchscreen of a kobo glo,
 * probably works with a range of /dev/input/event classs of
 * touchscreens.
 */

typedef struct Ts
{
  int    fd;
  double  x;
  double  y;
  int    down;
  int    rotate;
  int    width;
  int    height;
} Ts;

static Ts ts;
static Ts* this = &ts;

extern void *_mrg_evsrc_coord;

static int read_sys_int (const char *path)
{
  int ret = -1;
  int fd = open (path, O_RDONLY);
  char buf[16]="";
  if (fd>0)
    {
      if (read (fd, buf, 16)>0)
        ret = atoi (buf);
      close (fd);
    }
  return ret;
}


static int read_sys_int2 (const char *path, const char c)
{
  int ret = -1;
  int fd = open (path, O_RDONLY);
  char buf[32]="";
  if (fd>0)
    {
      if (read (fd, buf, 32)>0)
        {
          char *p = strchr (buf, c);
          if (p)
            ret = atoi (p+1);
        }
      close (fd);
    }
  return ret;
}

static int ufb_evsource_ts_init ()
{
  /* need to detect which event */

  this->down = 0;
  this->fd = open ("/dev/input/event1", O_RDONLY | O_NONBLOCK);

  this->rotate = read_sys_int ("/sys/class/graphics/fb0/rotate");
  if (this->rotate < 0)
    this->rotate = 0;
  if (this->rotate)
    {
      this->width = read_sys_int2 ("/sys/class/graphics/fb0/modes", ':');
      this->height = read_sys_int2 ("/sys/class/graphics/fb0/modes", 'x');
    }

  if (this->fd == -1)
  {
    fprintf (stderr, "error opening zforce device\n");
    //sleep (1);
    return -1;
  }
  _mrg_evsrc_coord = this;
  return 0;
}

static void destroy ()
{
  if (this->fd != -1)
    close (this->fd);
}

static int has_event ()
{
  struct timeval tv;
  int retval;

  if (this->fd == -1)
    return 0;

  fd_set rfds;
  FD_ZERO (&rfds);
  FD_SET(this->fd, &rfds);
  tv.tv_sec = 0; tv.tv_usec = 0;
  retval = select (this->fd+1, &rfds, NULL, NULL, &tv);
  if (retval == 1)
    return FD_ISSET (this->fd, &rfds);
  return 0;
}

static char *get_event ()
{
  const char *ret = "mouse-motion";
  struct input_event ev;
  int buttstate = 0;
  int sync = 0;

  while (!sync){
    int rc = read (this->fd, &ev, sizeof (ev));
    if (rc != sizeof (ev))
    {
      //fprintf (stderr, "zforce read fail\n");
      return NULL;
    }
    else
    {
      switch (ev.type)
      {
        case 3:
          switch (ev.code)
            {
              case 0: this->x = ev.value; break;
              case 1: this->y = ev.value; break;
              default: break;
            }
          break;
        case 1:
          switch (ev.code)
          {
            case 0x014a:
              switch (ev.value)
                {
                  case 0: ret = "mouse-release";
                          this->down = 0;
                     break;
                  case 1: ret = "mouse-press";
                          this->down = 1;
                     break;
                }
              buttstate = 1;
              break;
          }
          break;
        case 0: /* sync (end of packet) */
          sync = 1;
          break;
      }
    }
  }

  if (!buttstate && this->down)
    ret = "mouse-drag";

  {
    char *r = malloc (64);
    switch (this->rotate)
    {
      case 1:
        sprintf (r, "%s %.0f %.0f", ret, this->y, this->height-this->x);
        break;
      case 2:
        sprintf (r, "%s %.0f %.0f", ret, this->height-this->x, this->width-this->y);
        break;
      case 3:
        sprintf (r, "%s %.0f %.0f", ret, this->height-this->y, this->x);
        break;
      case 0:
      default:
        sprintf (r, "%s %.0f %.0f", ret, this->x, this->y);
        break;
    }
    return r;
  }

  return NULL;
}

static int get_fd (EvSource *ev_source)
{
  return this->fd;
}

static void set_coord (EvSource *ev_source, double x, double y)
{
  fprintf (stderr, "can't really warp on a touch screen..\n");
  /* synthesising a move event might do part of it though - for a reset */
}

static EvSource src = {
  NULL,
  (void*)has_event,
  (void*)get_event,
  (void*)destroy,
  get_fd,
  set_coord
};

EvSource *evsource_ts_new (void)
{
  if (ufb_evsource_ts_init () == 0)
    {
      return &src;
    }
  return NULL;
}
