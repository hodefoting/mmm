#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include "mmfb.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include "mmfb-backend.h"
#include <sys/stat.h>
#include <errno.h>

typedef struct _Client Client;
typedef struct _Host      Host;
typedef struct _HostSDL   HostSDL;

int quit = 0;

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

struct _HostSDL
{
  Host         host;
  SDL_Surface *screen;
};

static void host_clear_dirt (Host *host)
{
  host->dirty_x0 = 10000;
  host->dirty_y0 = 10000;
  host->dirty_x1 = -10000;
  host->dirty_y1 = -10000;
}

static void host_add_dirt (Host *host, int x0, int y0, int x1, int y1)
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
  fprintf (stderr, "incoming client %s\n", client_name);

  {
    Client *client = calloc (sizeof (Client), 1);

    char tmp[256];
    sprintf (tmp, "%s/%s", host->fbdir, client_name);
    client->ufb = ufb_host_open (tmp);
    client->pid = ufb_client_pid (client->ufb);

    if (client->pid == 0)
    {
      ufb_destroy (client->ufb);
      free (client);
      return;
    }

    fprintf (stderr, "new client %li\n", client->pid);

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
        static int pos = 0;
        ufb_set_x (client->ufb, pos);
        ufb_set_y (client->ufb, pos);

        //pos += 12;
      }
    }
    host->client = client;

    fprintf (stderr, "new client!\n");
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

void host_queue_draw (Host *host, UfbRectangle *rect)
{
  if (rect)
    host_add_dirt (host, rect->x, rect->y, rect->x+rect->width, rect->y+rect->height);
  else
    host_add_dirt (host, 0, 0, host->width, host->height);
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

      fprintf (stderr, "removed client %li\n", client->pid);
      unlink (tmp);
      quit = 1;
      free (client);
      host->client = NULL;

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

static void render_client (Host *host, Client *client, float ptr_x, float ptr_y)
{
  HostSDL *host_sdl = (void*)host;
  SDL_Surface *screen = host_sdl->screen;
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

  x = ufb_get_x (client->ufb);
  y = ufb_get_y (client->ufb);

  if (pixels && width && height)
  {
    int front_offset = y * host->stride + x * host->bpp;
    uint8_t *dst = screen->pixels + front_offset;
    const uint8_t *src = pixels;
    int copystride = host->stride - x * host->bpp;
    int scan;
    if (copystride > width * host->bpp)
    {
      copystride = width * host->bpp;
    }
    for (scan = 0; scan < height; scan ++)
    {
      if (dst >= (uint8_t*)screen->pixels &&
          dst < ((uint8_t*)screen->pixels) + (host->stride * host->height) - copystride)
        memcpy (dst, src, copystride);
      dst += host->stride;
      src += rowstride;
    }

    ufb_read_done (client->ufb);
  }
  SDL_UpdateRect(screen, 0,0,0,0);

  ufb_host_get_size (client->ufb, &cwidth, &cheight);

  if ( (cwidth  && cwidth != host->width) ||
       (cheight && cheight != host->height))
  {
    fprintf (stderr, "%i, %i\n", cwidth, cheight);

    host_sdl->screen = SDL_SetVideoMode (cwidth,
                                         cheight,32,
                                         SDL_SWSURFACE | SDL_RESIZABLE);
    host->width = cwidth;
    host->height = cheight;
    host->stride = host->width * host->bpp;
#if 0
    if (host->client)
       ufb_host_set_size (host->client->ufb,
          host->width, host->height);
#endif
  }
}

void host_destroy (Host *host)
{
  free (host);
}

static int host_idle_check (void *data)
{
  Host *host = data;
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

static void mmfb_sdl_fullscreen (Host *host, int fullscreen)
{
  HostSDL *host_sdl = (void*)host;
  SDL_Surface *screen = host_sdl->screen;
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
    host_sdl->screen = screen;
  }
  else
  {
    screen = SDL_SetVideoMode(width, height, 32,
                              SDL_SWSURFACE | SDL_RESIZABLE );
    host_sdl->screen = screen;
  }
  host->width = width;
  host->bpp = 4;
  host->stride = host->width * host->bpp;
  host->height = height;
  host->fullscreen = fullscreen;
}

Host *host_sdl_new (const char *path, int width, int height)
{
  Host *host = calloc (sizeof (HostSDL), 1);
  HostSDL *host_sdl = (void*)host;

  if (SDL_Init(SDL_INIT_VIDEO) < 0)
  {
    fprintf (stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
    return NULL;
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

  host_sdl->screen =  SDL_SetVideoMode (host->width, host->height, 32, SDL_SWSURFACE | SDL_RESIZABLE);

  if (host->fullscreen)
    mmfb_sdl_fullscreen (host, 1);

  if (!host_sdl->screen)
  {
    fprintf (stderr, "Unable to create display surface: %s\n", SDL_GetError());
    return NULL;
  }

  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
  SDL_EnableUNICODE(1);

  host_clear_dirt (host);
  host->fbdir = strdup (path);
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

int main (int argc, char **argv)
{
  Host *host;
  const char *path = "/tmp/ufb";
  setenv ("UFB_PATH", path, 1);

  if (argv[1] == NULL)
    goto a; /* presuming we've been invoked by a client  */

  if (argv[1])
  {
    if (argv[1] && argv[1][0]=='-')
    {
      /* might be started with an argument by a client creating the host on
       * demand
       */
    }

    switch (fork())
    {
      case 0:
        {
a:
          setenv ("UFB_IS_COMPOSITOR", "foo", 1);
          host = host_sdl_new (path, 640, 480);
          HostSDL *host_sdl = (void*)host;
          host_sdl = (void*) host;
          unsetenv ("UFB_IS_COMPOSITOR");

          SDL_Event event;

          atexit (SDL_Quit);

          while (!quit)
          {
            int got_event = 0;
            char buf[64];
            while (SDL_PollEvent (&event))
            {
              switch (event.type)
              {
                case SDL_MOUSEMOTION:
                  {
                    if (host->pointer_down[0])
                    sprintf (buf, "mouse-drag %.0f %.0f",
                         (float)event.motion.x,
                         (float)event.motion.y);
                    else
                    sprintf (buf, "mouse-motion %.0f %.0f",
                         (float)event.motion.x,
                         (float)event.motion.y);
                    if (host->client)
                      ufb_add_event (host->client->ufb, buf);
                  }
                  break;
                case SDL_MOUSEBUTTONDOWN:
                  {
                    sprintf (buf, "mouse-press %.0f %.0f",
                         (float)event.button.x,
                         (float)event.button.y);
                    if (host->client)
                      ufb_add_event (host->client->ufb, buf);
                    host->pointer_down[0] = 1;
                  }
                  break;
                case SDL_MOUSEBUTTONUP:
                  {
                    sprintf (buf, "mouse-release %.0f %.0f",
                         (float)event.button.x,
                         (float)event.button.y);
                    if (host->client)
                      ufb_add_event (host->client->ufb, buf);
                    host->pointer_down[0] = 0;
                  }
                  break;
                case SDL_KEYDOWN:
                  {
                    char buf[64] = "";
                    char *name = NULL;

                    buf[mmfb_unichar_to_utf8 (event.key.keysym.unicode, (void*)buf)]=0;
                    switch (event.key.keysym.sym)
                    {
                      case SDLK_F1:        name = "F1";break;
                      case SDLK_F2:        name = "F2";break;
                      case SDLK_F3:        name = "F3";break;
                      case SDLK_F4:        name = "F4";break;
                      case SDLK_F5:        name = "F5";break;
                      case SDLK_F6:        name = "F6";break;
                      case SDLK_F7:        name = "F7";break;
                      case SDLK_F8:        name = "F8";break;
                      case SDLK_F9:        name = "F9";break;
                      case SDLK_F10:       name = "F10";break;
                      case SDLK_F11:       name = "F11";break;
                      case SDLK_F12:       name = "F12";break;
                      case SDLK_ESCAPE:    name = "escape";break;
                      case SDLK_DOWN:      name = "down";break;
                      case SDLK_LEFT:      name = "left";break;
                      case SDLK_UP:        name = "up";break;
                      case SDLK_RIGHT:     name = "right";break;
                      case SDLK_BACKSPACE: name = "backspace";break;
                      case SDLK_TAB:       name = "tab";break;
                      case SDLK_DELETE:    name = "delete";break;
                      case SDLK_INSERT:    name = "insert";break;
                      case SDLK_RETURN:    name = "return";break;
                      case SDLK_HOME:      name = "home";break;
                      case SDLK_END:       name = "end";break;
                      case SDLK_PAGEDOWN:  name = "page-down";   break;
                      case SDLK_PAGEUP:    name = "page-up"; break;

                      default:
                        if (event.key.keysym.unicode < 32)
                        {
                          buf[0] = event.key.keysym.unicode;
                          buf[1] = 0;
                        }
                        name = (void*)&buf[0];
                    }
                    if (event.key.keysym.mod & (KMOD_CTRL))
                    {
                      char buf2[64] = "";
                      sprintf (buf2, "control-%c", event.key.keysym.sym);
                      name = buf2;
                      if (event.key.keysym.mod & (KMOD_SHIFT))
                      {
                        char buf2[64] = "";
                        sprintf (buf2, "shift-%c", event.key.keysym.sym);
                        name = buf2;
                      }
                    }
                    if (event.key.keysym.mod & (KMOD_ALT))
                    {
                      char buf2[64] = "";
                      sprintf (buf2, "alt-%c", event.key.keysym.sym);
                      name = buf2;
                      if (event.key.keysym.mod & (KMOD_SHIFT))
                      {
                        char buf2[64] = "";
                        sprintf (buf2, "shift-%c", event.key.keysym.sym);
                        name = buf2;
                      }
                    }
                    if (name)
                      if (host->client)
                        ufb_add_event (host->client->ufb, name);
                  }
                  break;
                case SDL_VIDEORESIZE:
                  host_sdl->screen = SDL_SetVideoMode (event.resize.w,
                                                   event.resize.h,32,
                                                   SDL_SWSURFACE | SDL_RESIZABLE);
                  host->width = event.resize.w;
                  host->height = event.resize.h;
                  host->stride = host->width * host->bpp;
                  if (host->client)
                    ufb_host_set_size (host->client->ufb,
                        host->width, host->height);
                  break;
              }
              got_event = 1;
            }
            if (!got_event)
            {
              usleep (10000);
            }
            host_idle_check (host);
            host_monitor_dir (host);

            if (host->client)
              render_client (host, host->client, 0, 0);

            host_clear_dirt (host);
          }
          return 0;
        }
      case -1:
        fprintf (stderr, "fork failed\n");
        return 0;
    }
  }
  execvp (argv[1], argv+1);
  return 0;
}
