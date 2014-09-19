#include "mmfb.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>


typedef struct _Client Client;
typedef struct _Host   Host;

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

#define TITLE_BAR_HEIGHT 16

struct _Host
{
  char   *fbdir;
  Client *client;
  Client *focused;
};


Host *host;

Host *host_new (void)
{
  Host *host = calloc (sizeof (Host), 1);
  return host;
}


static void validate_client (Host *host, const char *client_name)
{
  if (host->client)
  {
    Client *client = host->client;
    if (client->filename &&
        !strcmp (client->filename, client_name))
    {
      return;
    }
  }

  {
    Client *client = calloc (sizeof (Client), 1);

    char tmp[256];
    sprintf (tmp, "%s/%s", host->fbdir, client_name);
    client->ufb = ufb_host_open (tmp);
    client->pid = ufb_client_pid (client->ufb);

    if (!client->ufb)
    {
      fprintf (stderr, "failed to open client %s\n", tmp);
      return;
    }

    client->filename = strdup (client_name);

    if (client->pid != getpid ())
    {
      if (ufb_get_x (client->ufb) == 0 &&
          ufb_get_y (client->ufb) == 0)
      {
        static int pos = 10;
        ufb_set_x (client->ufb, pos);
        ufb_set_y (client->ufb, 30+pos);

        pos += 12;
      }
    }
    host->client = client;
  }
}

#include <sys/stat.h>
#include <errno.h>


static int pid_is_alive (long pid)
{
  char path[256];
  struct stat sts;
  sprintf (path, "/proc/%li", pid);
  if (stat(path, &sts) == -1 && errno == ENOENT) {
    return 0;
  }
  return 1;
}

void host_queue_draw (Host *host, UfbRectangle *rect)
{
//XXX
}

static void host_monitor_dir (Host *host)
{
again:
  if (host->client)
  {
    Client *client = host->client;
    if (!pid_is_alive (client->pid))
    {
      char tmp[256];
      sprintf (tmp, "%s/%s", host->fbdir, client->filename);
      if (client->ufb)
      {
        ufb_destroy (client->ufb);
        client->ufb = NULL;
      }

      unlink (tmp);
      host_queue_draw (host, NULL);

      free (client);
      host->client = NULL;
      goto again;
    }
  }

  DIR *dir = opendir (host->fbdir);
  struct dirent *ent;
  static int iteration = 0;

  iteration ++;

  while ((ent = readdir (dir)))
  {
    if (ent->d_name[0]!='.')
      validate_client (host, ent->d_name);
  }
  closedir (dir);
}

static void render_client (Host *host, Client *client, float ptr_x, float ptr_y)
{
#if MRG_CAIRO
  cairo_t *cr = mrg_cr (mrg);
  cairo_surface_t *surface;
#endif
  int width, height, rowstride;

  if (client->pid == getpid ())
    return;

  int cwidth, cheight;

  const unsigned char *pixels = ufb_get_buffer_read (client->ufb,
      &width, &height, &rowstride);
  int x, y;

  ufb_host_get_size (client->ufb, &cwidth, &cheight);

  x = ufb_get_x (client->ufb);
  y = ufb_get_y (client->ufb);

  if (ptr_x >= x && ptr_x < x + width &&
      ptr_y >= y - TITLE_BAR_HEIGHT && ptr_y < y + height)
  {
    host->focused = client;
  }

  if (pixels)
  {
#if MRG_CAIRO
    surface = cairo_image_surface_create_for_data ((void*)pixels,               CAIRO_FORMAT_ARGB32, width, height, rowstride);
    cairo_save (cr);
    cairo_translate (cr, x, y);
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint (cr);
    cairo_surface_destroy (surface);
#endif
    ufb_read_done (client->ufb);

#if MRG_CAIRO
    cairo_restore (cr);
#endif
  }
}

static void render_ui (void *data)
{
  Host *host = data;
  host_monitor_dir (host);
  host->focused = NULL;

  if (host->client)
    render_client (host, host->client, 0, 0);
}

static void init_env (Host *host)
{
  char buf[512];
  if (host->fbdir)
    return;
  host->fbdir = "/tmp/ufb";
  if (getenv ("UFB_PATH"))
    host->fbdir = getenv ("UFB_PATH");
  else
    setenv ("UFB_PATH", host->fbdir, 1);
  sprintf (buf, "mkdir %s &> /dev/null", host->fbdir);
  system (buf);
}

void host_destroy (Host *host)
{
  free (host);
}

static int host_idle_check (void *data)
{
  Host *host = data;

  //
  if (host->client)
  {
    Client *client = host->client;

    int x, y, width, height;
    if (ufb_get_damage (client->ufb, &x, &y, &width, &height))
    {
      if (width)
      {
        UfbRectangle rect = {x + ufb_get_x (client->ufb), ufb_get_y (client->ufb), width, height};
        //fprintf (stderr, "%i %i %i %i",  x, y, width, height);
        host_queue_draw (host, &rect);
      }
      else
      {
        UfbRectangle rect = {ufb_get_x (client->ufb), ufb_get_y (client->ufb),
                             ufb_get_width (client->ufb), ufb_get_height        (client->ufb)};
        host_queue_draw (host, &rect);
      }
    }

    while (ufb_has_message (client->ufb))
    {
      fprintf (stderr, "%p: %s\n", client->ufb, ufb_get_message (client->ufb));
    }
  }
  return 1;
}


int main (int argc, char **argv)
{
  setenv ("UFB_IS_COMPOSITOR", "foo", 1);
  host = host_new ();
  init_env (host);
  unsetenv ("UFB_IS_COMPOSITOR");

  if (argv[1] && argv[1]=='-')
  {
    /* might be started with an argument by a client creating the host on demand */
  }

  switch (fork())
  {
    case 0:
      execvp (argv[1], argv+1);
    case -1:
      fprintf (stderr, "fork failed\n");
      return 0;
  }

  sleep (1); // XXX: how to wait for host? perhaps mkdir?
  fprintf (stderr, "host hi\n");
  return 0;
}
