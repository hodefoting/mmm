#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "ufb.h"
#include "ufb-evsource.h"

/* written to work with the zforce ir touchscreen of a kobo glo,
 * probably works with a range of /dev/input/event classs of
 * touchscreens.
 */

typedef struct Mice
{
  int     fd;
  double  x;
  double  y;
  int     prev_state;
} Mice;

Mice *_mrg_evsrc_coord = NULL;

void _ufb_get_coords (Ufb *ufb, double *x, double *y)
{
  if (!_mrg_evsrc_coord)
    return;
  if (x)
    *x = _mrg_evsrc_coord->x;
  if (y)
    *y = _mrg_evsrc_coord->y;
}

static Mice  mice;
static Mice* mrg_mice_this = &mice;

static int ufb_evsource_mice_init ()
{
  unsigned char reset[]={0xff};
  /* need to detect which event */

  mrg_mice_this->prev_state = 0;
  mrg_mice_this->fd = open ("/dev/input/mice", O_RDONLY | O_NONBLOCK);
  if (mrg_mice_this->fd == -1)
  {
    fprintf (stderr, "error opening mice device\n");
//    sleep (1);
    return -1;
  }
    fprintf (stderr, "!\n");
  write (mrg_mice_this->fd, reset, 1);
  _mrg_evsrc_coord = mrg_mice_this;
  return 0;
}

static void mice_destroy ()
{
  if (mrg_mice_this->fd != -1)
    close (mrg_mice_this->fd);
}

static int mice_has_event ()
{
  struct timeval tv;
  int retval;

  if (mrg_mice_this->fd == -1)
    return 0;

  fd_set rfds;
  FD_ZERO (&rfds);
  FD_SET(mrg_mice_this->fd, &rfds);
  tv.tv_sec = 0; tv.tv_usec = 0;
  retval = select (mrg_mice_this->fd+1, &rfds, NULL, NULL, &tv);
  if (retval == 1)
    return FD_ISSET (mrg_mice_this->fd, &rfds);
  return 0;
}

static char *mice_get_event ()
{
  const char *ret = "mouse-motion";
  double relx, rely;
  signed char buf[3];
  read (mrg_mice_this->fd, buf, 3);
  relx = buf[1];
  rely = -buf[2];

  mrg_mice_this->x += relx;
  mrg_mice_this->y += rely;

  if ((mrg_mice_this->prev_state & 1) != (buf[0] & 1))
    {
      if (buf[0] & 1)
        {
          ret = "mouse-press";
        }
      else
        {
          ret = "mouse-release";
        }
    }
  else if (buf[0] & 1)
    ret = "mouse-drag";

  mrg_mice_this->prev_state = buf[0];

  {
    char *r = malloc (64);
    sprintf (r, "%s %.0f %.0f", ret, mrg_mice_this->x, mrg_mice_this->y);
    return r;
  }

  return NULL;
}

static int mice_get_fd (EvSource *ev_source)
{
  return mrg_mice_this->fd;
}

static void mice_set_coord (EvSource *ev_source, double x, double y)
{
  mrg_mice_this->x = x;
  mrg_mice_this->y = y;
}

static EvSource ev_src_mice = {
  NULL,
  (void*)mice_has_event,
  (void*)mice_get_event,
  (void*)mice_destroy,
  mice_get_fd,
  mice_set_coord
};

EvSource *evsource_mice_new (void)
{
  if (ufb_evsource_mice_init () == 0)
    {
      mrg_mice_this->x = 0;
      mrg_mice_this->y = 0;
      return &ev_src_mice;
    }
  return NULL;
}
