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

#ifndef HOST_H
#define HOST_H

#include "mmm-list.h"

typedef struct _Client    Client;
typedef struct _Host      Host;

struct _Client
{
  char *filename;
  Mmm  *mmm;
  long  pid;

  int  premax_x;
  int  premax_y;
  int  premax_width;
  int  premax_height;
};
struct _Host
{
  char        *fbdir;
  MmmList     *clients;
  Client      *focused;
  int          fullscreen;
  int          dirty_xmin;
  int          dirty_ymin;
  int          dirty_xmax;
  int          dirty_ymax;

  int          clip_xmin;
  int          clip_xmax;
  int          clip_ymin;
  int          clip_ymax;

  int          width;
  int          bpp;
  int          stride;
  int          height;
  int          pointer_down[8];

  int          single_app;
};

void host_clear_dirt  (Host *host);
void host_add_dirt    (Host *host, int x0, int y0, int x1, int y1);
void validate_client  (Host *host, const char *client_name);
void host_queue_draw  (Host *host, MmmRectangle *rect);
void host_monitor_dir (Host *host);
int  host_idle_check  (void *data);
int  host_is_dirty    (Host *host);

extern int host_has_quit;
extern int host_width;
extern int host_height;

#endif
