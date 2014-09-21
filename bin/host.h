#ifndef HOST_H
#define HOST_H

typedef struct _Client    Client;
typedef struct _Host      Host;


struct _Client
{
  char *filename;
  Ufb  *ufb;
  long  pid;

  int  premax_x;
  int  premax_y;
  int  premax_width;
  int  premax_height;
};
struct _Host
{
  char        *fbdir;
  Client      *client;
  int          fullscreen;
  int          dirty_x0;
  int          dirty_y0;
  int          dirty_x1;
  int          dirty_y1;
  int          width;
  int          bpp;
  int          stride;
  int          height;
  int          pointer_down[8];
};



void host_clear_dirt (Host *host);
void host_add_dirt (Host *host, int x0, int y0, int x1, int y1);
void validate_client (Host *host, const char *client_name);
void host_queue_draw (Host *host, UfbRectangle *rect);
void host_monitor_dir (Host *host);
int host_idle_check (void *data);

extern int host_has_quit;

#endif
