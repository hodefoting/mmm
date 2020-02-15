/*
 * 2014, 2020 (c) Øyvind Kolås
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

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include "mmm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <SDL.h>
#include <sys/stat.h>
#include <errno.h>
#include "host.h"

typedef struct _HostSDL   HostSDL;

struct _HostSDL
{
  Host          host;
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Event     event;
  SDL_Texture  *texture;
};

static void render_client (Host *host, Client *client, float ptr_x, float ptr_y)
{
  HostSDL *host_sdl = (void*)host;
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

  //mmm_host_get_size (client->mmm, &cwidth, &cheight);
  if (pixels && width && height)
  {
    if (host->width != width || host->height != height)
    {
      //SDL_SetWindowSize (host_sdl->window, width, height);
      //   window size should only be set...
      //   if it is the client and not the user changing size
      host->width = width;
      host->height = height;
      SDL_DestroyTexture (host_sdl->texture);
      host_sdl->texture = SDL_CreateTexture(host_sdl->renderer,
                               SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               width, height);
    }

#if 0
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom((void*)pixels,
		                    width, height, 4, width * 4, SDL_PIXELFORMAT_ARGB8888);
#endif
    //SDL_Texture *texture = SDL_CreateTextureFromSurface(host_sdl->renderer, surface);
    SDL_UpdateTexture(host_sdl->texture, NULL, (void*)pixels, width * sizeof (Uint32));

    SDL_RenderClear(host_sdl->renderer);
    SDL_RenderCopy(host_sdl->renderer, host_sdl->texture, NULL, NULL);
    SDL_RenderPresent(host_sdl->renderer);
    //SDL_DestroyTexture (texture);
    //SDL_FreeSurface (surface);
    mmm_read_done (client->mmm);
  }

  mmm_host_get_size (client->mmm, &cwidth, &cheight);

  if (host->single_app)
  {
    #if 0
    static int old_baseflags = 0;

    if (mmm_get_value (client->mmm, "borderless"))
      baseflags |= SDL_NOFRAME;
    else
    {
      if (baseflags & SDL_NOFRAME)
        baseflags -= SDL_NOFRAME;
    }
    if ( (cwidth  && (cwidth  != host->width)) ||
         (cheight && (cheight != host->height)) ||
         (old_baseflags != baseflags))
    {
      host->width = cwidth;
      host->height = cheight;
      host->stride = host->width * host->bpp;
      host_sdl->screen = SDL_SetVideoMode (host->width, host->height, 32,
                                           baseflags | SDL_RESIZABLE);
      old_baseflags = baseflags;
    }
#endif
  }
}

void host_destroy (Host *host)
{
  free (host);
}

static void mmfb_sdl_fullscreen (Host *host, int fullscreen)
{
#if 0
  HostSDL *host_sdl = (void*)host;
  SDL_Surface *screen = host_sdl->screen;
  int width = 1024, height = 768;

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
                              baseflags | SDL_FULLSCREEN );
    host_sdl->screen = screen;
  }
  else
  {
    screen = SDL_SetVideoMode(width, height, 32,
                              baseflags | SDL_RESIZABLE );
    host_sdl->screen = screen;
  }
  host->width = width;
  host->bpp = 4;
  host->stride = host->width * host->bpp;
  host->height = height;
  host->fullscreen = fullscreen;
#endif
}

Host *host_sdl_new (const char *path, int width, int height)
{
  Host *host = calloc (sizeof (HostSDL), 1);
  HostSDL *host_sdl = (void*)host;

  if (width < 0)
  {
    width = host_width = 512;
    height = host_height = 384;
  }

  host_width = width;
  host_height = height;
  host_sdl->window = SDL_CreateWindow("mmm", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
  host_sdl->renderer = SDL_CreateRenderer(host_sdl->window, -1, 0);
  SDL_StartTextInput();

  host_sdl->texture = SDL_CreateTexture(host_sdl->renderer,
                               SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               width, height);


#if 0
  if (width < 0)
  {
    SDL_Rect **modes;
    modes = SDL_ListModes(NULL, SDL_HWSURFACE|SDL_FULLSCREEN);
    host->fullscreen = 1;
    if (modes == (SDL_Rect**)0) {
      width=host_width = 400;
      height=host_height = 300;
    }
    else
    {
      width=host_width = modes[0]->w - 128;
      height=host_height = modes[0]->h - 128;
    }

  }
#endif

  host->width = width;
  host->bpp = 4;
  host->stride = host->width * host->bpp;
  host->height = height;

  //host_sdl->screen =  SDL_SetVideoMode (host->width, host->height, 32, baseflags | SDL_RESIZABLE);

  if (host->fullscreen)
    mmfb_sdl_fullscreen (host, 1);

  //if (!host_sdl->screen)
  //{
  //  fprintf (stderr, "Unable to create display surface: %s\n", SDL_GetError());
  //  return NULL;
  //}

//  SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
//  SDL_EnableUNICODE(1);

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

static int lctrl = 0;
static int rctrl = 0;

static int sdl_check_events (Host *host)
{
  //HostSDL *host_sdl = (void*)host;
  SDL_Event event;
  int got_event = 0;
  char buf[64];
  while (SDL_PollEvent (&event))
  {
    //const uint8_t *state = SDL_GetKeyboardState(NULL);

    switch (event.type)
    {
      case SDL_WINDOWEVENT:
	{
	  if (event.window.event == SDL_WINDOWEVENT_RESIZED)
	  {
            host->width  = event.window.data1;
            host->height = event.window.data2;
            host->stride = host->width * host->bpp;

            if (host->single_app && host->focused)
	    {
               mmm_host_set_size (host->focused->mmm,
                             host->width, host->height);

               //host_add_dirt (host, 0,0,host->width, host->height);
	    }
	  }
	}
	break;

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

          if (host->focused)
            mmm_add_event (host->focused->mmm, buf);
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        {
          sprintf (buf, "mouse-press %.0f %.0f",
               (float)event.button.x,
               (float)event.button.y);
          if (host->focused)
            mmm_add_event (host->focused->mmm, buf);
          host->pointer_down[0] = 1;
        }
        break;
      case SDL_MOUSEBUTTONUP:
        {
          sprintf (buf, "mouse-release %.0f %.0f",
               (float)event.button.x,
               (float)event.button.y);

          if (host->focused)
            mmm_add_event (host->focused->mmm, buf);
          host->pointer_down[0] = 0;
        }
        break;
      case SDL_TEXTINPUT:
	if (!lctrl && !rctrl) {
	  const char *name = event.text.text;
	  if (!strcmp (name, " ")) name = "space";

          if (host->focused)
	  {
            mmm_add_event (host->focused->mmm, name);
	  }
	}
	break;
      case SDL_KEYUP:
	{
          switch (event.key.keysym.sym)
          {
            case SDLK_LCTRL:     lctrl=0;      break;
            case SDLK_RCTRL:     rctrl=0;      break;
	  }
	}
	break;
      case SDL_KEYDOWN:
        {
          char buf[64] = "";
          char *name = buf;

          buf[mmfb_unichar_to_utf8 (event.key.keysym.sym, (void*)buf)]=0;

          switch (event.key.keysym.sym)
          {
            case SDLK_LCTRL:     lctrl=1;      break;
            case SDLK_RCTRL:     rctrl=1;      break;
            case SDLK_F1:        name = "F1";       break;
            case SDLK_F2:        name = "F2";       break;
            case SDLK_F3:        name = "F3";       break;
            case SDLK_F4:        name = "F4";       break;
            case SDLK_F5:        name = "F5";       break;
            case SDLK_F6:        name = "F6";       break;
            case SDLK_F7:        name = "F7";       break;
            case SDLK_F8:        name = "F8";       break;
            case SDLK_F9:        name = "F9";       break;
            case SDLK_F10:       name = "F10";      break;
            case SDLK_F11:       name = "F11";      break;
            case SDLK_F12:       name = "F12";      break;
            case SDLK_ESCAPE:    name = "escape";   break;
            case SDLK_DOWN:      name = "down";     break;
            case SDLK_LEFT:      name = "left";     break;
            case SDLK_UP:        name = "up";       break;
            case SDLK_RIGHT:     name = "right";    break;
            case SDLK_BACKSPACE: name = "backspace";break;
            case SDLK_SPACE:     name = "space";    break;
            case SDLK_TAB:       name = "tab";      break;
            case SDLK_DELETE:    name = "delete";   break;
            case SDLK_INSERT:    name = "insert";   break;
            case SDLK_RETURN:    name = "return";   break;
            case SDLK_HOME:      name = "home";     break;
            case SDLK_END:       name = "end";      break;
            case SDLK_PAGEDOWN:  name = "page-down";break;
            case SDLK_PAGEUP:    name = "page-up";  break;

            default:;
          }

	  if (strlen(name)){
          if (event.key.keysym.mod & (KMOD_CTRL) ||
              event.key.keysym.mod & (KMOD_ALT) ||
              strlen (name) >= 2)
          {

          if (event.key.keysym.mod & (KMOD_CTRL))
          {
            static char buf[64] = "";
            sprintf (buf, "control-%s", name);
            name = buf;
          }
          if (event.key.keysym.mod & (KMOD_ALT))
          {
            static char buf[64] = "";
            sprintf (buf, "alt-%s", name);
            name = buf;
          }
          if (event.key.keysym.mod & (KMOD_SHIFT))
          {
            static char buf[64] = "";
            sprintf (buf, "shift-%s", name);
            name = buf;
          }

	    if (strcmp (name, "space"))
	    {
              if (host->focused)
                mmm_add_event (host->focused->mmm, name);
	    }
          }
	  }
        }
        break;
    }
    got_event = 1;
  }
  return got_event;
}

static int main_sdl (const char *path, int single)
{
  Host *host;
  host     = host_sdl_new (path, 1024, 768);
  HostSDL *host_sdl = (void*)host;
  //host_sdl = (void*) host;
  atexit (SDL_Quit);
  host->single_app = single;

  audio_init_alsa (host); // XXX : look into implement audio output using SDL2 itself instead

  int sleep_time = 500;

  int iter = 0;
  while (!host_has_quit)
  {
    int got_event = 0;

    got_event = sdl_check_events (host);
    host_idle_check (host); // this update if the host is dirty

    if (iter ++ % 10 == 0) // we do not need to do it frequently
      host_monitor_dir (host);

    if (got_event)
      sleep_time = 10000;

    if (host_is_dirty (host)) {
      int x, y;
      SDL_GetMouseState(&x, &y);

      if (!host->single_app)
        host->focused = NULL;

      MmmList *l;
      for (l = host->clients; l; l = l->next)
      {
        render_client (host, l->data, x, y);
      }
      host_clear_dirt (host);
      got_event = 1;
    }
    //else
    {
      if (host->single_app && !host->focused)
        host->focused = host->clients?host->clients->data:NULL;
      if (host->single_app && host->focused)
      {
        static char *title = NULL;
        if (!title || strcmp (title, mmm_get_title (host->focused->mmm)))
        {
          if (title) free (title);
          title = strdup (mmm_get_title (host->focused->mmm));
	  SDL_SetWindowTitle (host_sdl->window, title);
          //SDL_WM_SetCaption (title, "mmm");
        }
      }
      if (got_event)
      {
	if (sleep_time > 30000)
	  sleep_time = 30000;
      }
      else
      {
	if (sleep_time > 1500000)
	  sleep_time = 1500000;
        usleep (sleep_time);
	sleep_time *= 1.5;
      }
    }
  }

  if (host->single_app)
  {
    rmdir (host->fbdir);
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
    return main_sdl (path, 0);
  }

  if (argv[1][0] == '-' &&
      argv[1][1] == 'p' &&
      argv[2])
  {
    return main_sdl (argv[2], 1);
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
        return main_sdl (path, 1);
      case -1:
        fprintf (stderr, "fork failed\n");
        return 0;
    }

  execvp (argv[1], argv+1);
  return 0;
}
