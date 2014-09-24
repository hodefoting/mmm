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

#include "mmm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "host.h"

int host_has_quit = 0;

void host_clear_dirt (Host *host)
{
  host->dirty_x0 = 10000;
  host->dirty_y0 = 10000;
  host->dirty_x1 = -10000;
  host->dirty_y1 = -10000;
}

void host_add_dirt (Host *host, int x0, int y0, int x1, int y1)
{
  if (x0 < host->dirty_x0)
    host->dirty_x0 = x0;
  if (y0 < host->dirty_y0)
    host->dirty_y0 = y0;
  if (x1 > host->dirty_x1)
    host->dirty_x0 = x0;
  if (y1 > host->dirty_y1)
    host->dirty_y1 = y1;
}

void validate_client (Host *host, const char *client_name)
{
  MmmList *l;
  for (l = host->clients; l; l = l->next)
  {
    Client *client = l->data;
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
    client->mmm = mmm_host_open (tmp);
    client->pid = mmm_client_pid (client->mmm);

    if (client->pid == 0)
    {
      mmm_destroy (client->mmm);
      free (client);
      return;
    }

    if (!client->mmm)
    {
      fprintf (stderr, "failed to open client %s\n", tmp);
      return;
    }

    client->filename = strdup (client_name);

    if (client->pid != getpid ())
    {
      if (mmm_get_x (client->mmm) == 0 &&
          mmm_get_y (client->mmm) == 0)
      {
        static int pos = 0;
        int width  = mmm_get_width (client->mmm);
        int height = mmm_get_width (client->mmm);

        if (width < 0 || height < 0)
        {
          mmm_host_set_size (client->mmm, host->width, host->height);
          mmm_set_x (client->mmm, 0);
          mmm_set_y (client->mmm, 0);
        }
        else
        {
          mmm_set_x (client->mmm, 0);
          mmm_set_y (client->mmm, 0);
          //mmm_set_x (client->mmm, (host->width - width) / 2);
          //mmm_set_y (client->mmm, (host->height - height) / 2);
        }

        //pos += 12;
      }
    }
    mmm_list_append (&host->clients, client);
  }
}

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

void host_queue_draw (Host *host, MmmRectangle *rect)
{
  if (rect)
    host_add_dirt (host, rect->x, rect->y, rect->x+rect->width, rect->y+rect->height);
  else
    host_add_dirt (host, 0, 0, host->width, host->height);
}

void host_monitor_dir (Host *host)
{
  MmmList *l;
again:
  for (l = host->clients; l; l = l->next)
  {
    Client *client = l->data;
    if (!pid_is_alive (client->pid))
    {
      char tmp[256];
      sprintf (tmp, "%s/%s", host->fbdir, client->filename);
      if (client->mmm)
      {
        mmm_destroy (client->mmm);
        client->mmm = NULL;
      }

      unlink (tmp);
      host_has_quit = 1;
      free (client);
      mmm_list_remove (&host->clients, client);


      host_queue_draw (host, NULL);
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

void host_window_raise (Host *host, Client *focused)
{
  if (!focused)
    return;

  mmm_list_remove (&host->clients, focused);
  mmm_list_append (&host->clients, focused);
}

int host_idle_check (void *data)
{
  Host *host = data;
  MmmList *l;
  for (l = host->clients; l; l = l->next)
  {
    Client *client = l->data;

    int x, y, width, height;
    if (mmm_get_damage (client->mmm, &x, &y, &width, &height))
    {
      if (width)
      {
        MmmRectangle rect = {x + mmm_get_x (client->mmm), mmm_get_y (client->mmm), width, height};
        //fprintf (stderr, "%i %i %i %i",  x, y, width, height);
        host_queue_draw (host, &rect);
      }
      else
      {
        MmmRectangle rect = {mmm_get_x (client->mmm), mmm_get_y (client->mmm),
                             mmm_get_width (client->mmm), mmm_get_height        (client->mmm)};
        host_queue_draw (host, &rect);
      }
    }

    while (mmm_has_message (client->mmm))
    {
      fprintf (stderr, "message: %p: %s\n", client->mmm, mmm_get_message (client->mmm));
    }
  }
  return 1;
}
