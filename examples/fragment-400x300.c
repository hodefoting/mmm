/* this is to serve as a minimal - no dependencies application
 * integrating with the oc display server.
 */
#include "mmm.h"
#include "mmm-pset.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

static int W = 400;
static int H = 300;

int tx = 0;
int ty = 0;

static inline int fastrand () { 
  static int g_seed = 23;
  g_seed = (214013*g_seed+2531011); 
  return (g_seed>>16)&0x7FFF; 
} 


typedef int (*Fragment)(int x, int y, int foo);


static inline int frag_constant (int x, int y, int foo) { return foo; }

static inline int frag_ripple (int x, int y, int foo)
{
  int u, v;
  u = x - W/2;
  v = y - H/2;
  return (((u * u + v * v)+foo) % (40000) < 20000) * 255;
}

static inline int frag_mandel (int hx, int hy, int foo)
{
  float cx,cy,xsq,ysq;
  unsigned int iter;
  int iterations = 64;
  float magnify = 1.4;
  float initx = (((float)hx)/((float)W)-0.5)/magnify*4.0-0.7;
  float inity = (((float)hy)/((float)H)-0.5)/magnify*3.0;
  
  cx=initx+initx*initx - inity*inity;   
  cy=inity+initx*inity+initx*inity; 
  
  for (iter=0;iter<iterations && (ysq=cy*cy)+(xsq=cx*cx)<4;iter++,cy=inity+cx*cy+cx*cy,cx=initx-ysq+xsq) ;
  return (iter&1) * 255;
}

static inline int frag_random (int x, int y, int foo)
{
  return fastrand () % 255;
}

static inline int frag_ripple_interference (int x, int y,int foo)
{
  return frag_ripple(x-(foo-512),y,0)^ frag_ripple(x+(foo-512),y,0);
}

static inline int frag_ripple_interference2 (int x, int y,int foo)
{
  return frag_ripple(x-(foo-512),y,0) + frag_ripple(x+(foo-512),y,0);
}

static inline int frag_hack2 (int x, int y,int foo)
{
  x+=foo * 5;
  x/=4;
  y/=4;
  return (x ^ y);
}

static inline int frag_hack (int x, int y,int foo)
{
  x/=4;
  y/=4;
  return (x ^ y) + foo;
}

void fill_random (Mmm *fb, int n)
{
  int x, y;
  unsigned char *buffer;
  int width, height, stride;
  int bpp;

  buffer = mmm_get_buffer_write (fb, &width, &height, &stride, NULL);

  bpp = mmm_get_bytes_per_pixel (fb);

  for (y = 0; y < height; y++)
  {
    unsigned char *pix = mmm_get_pix (fb, buffer, 0, y);
    for (x = 0; x < width; x++)
    {
      int val = fastrand()&0xff;
      pix = mmm_pix_pset (fb, pix, bpp, x, y, val, val, val, 255);
    }
  }

  mmm_write_done (fb, 0, 0, -1, -1);
}

void blank_it (Mmm *fb, int n)
{
  int x, y;
  unsigned char *buffer;
  int width, height, stride;
  int bpp;

  buffer = mmm_get_buffer_write (fb, &width, &height, &stride, NULL);

  bpp = mmm_get_bytes_per_pixel (fb);

  for (y = 0; y < height; y++)
  {
    unsigned char *pix = mmm_get_pix (fb, buffer, 0, y);
    for (x = 0; x < width; x++)
    {
      int val = 255;
      pix = mmm_pix_pset (fb, pix, bpp, x, y, val, val, val, 255);
    }
  }

  mmm_write_done (fb, 0, 0, -1, -1);
}


void fill_render (Mmm *fb, Fragment fragment, int foo)
{
  int x, y;
  unsigned char *buffer;
  int width, height, stride;
  int bpp;
  mmm_client_check_size (fb, &width, &height);

  buffer = mmm_get_buffer_write (fb, &width, &height, &stride, NULL);

  bpp = mmm_get_bytes_per_pixel (fb);

  for (y = 0; y < height; y++)
  {
    unsigned char *pix = mmm_get_pix (fb, buffer, 0, y);
    int u, v;
    u = 0 - (tx-width/2); v = y - (ty-height/2);
    for (x = 0; x < width; x++, u++)
    {
      int val = fragment (u, v, foo);
      pix = mmm_pix_pset (fb, pix, bpp, x, y, val, val, val, 255);
    }
  }

  mmm_write_done (fb, 0, 0, -1, -1);
}

void event_handling (Mmm *fb)
{
  while (mmm_has_event (fb))
    {
      const char *event = mmm_get_event (fb);
      float x = 0, y = 0;
      const char *p;
      p = strchr (event, ' ');
      if (p)
        {
          x = atof (p+1);
          p = strchr (p+1, ' ');
          if (p)
            y = atof (p+1);
        }
      tx = x;
      ty = y;
    }
}

void ghostbuster (Mmm *fb)
{
  int i;
  for (i = 0; i < 3; i++)
    {
      fill_render (fb, frag_random, i);
      usleep (140000);
    }
}

int main ()
{
  Mmm *fb = mmm_new (W, H, 0, NULL);
  int j;
  if (!fb)
    {
      fprintf (stderr, "failed to open buffer\n");
      return -1;
    }

  W = mmm_get_width (fb);
  H = mmm_get_height (fb);

  tx = W/2;
  ty = H/2;

  for (j = 0; j < 2; j ++)
  {
    int i;
    
    event_handling (fb);
    for (i = 0; i < 64; i+=2)
      {
        fill_render (fb, frag_ripple_interference, i);
        event_handling (fb);
      }
    for (i = 64; i < 96; i+=2)
      {
        fill_render (fb, frag_ripple_interference2, i);
        event_handling (fb);
      }
    {

  //mmm_set_size (fb, 64, 64);
  W = mmm_get_width (fb);
  H = mmm_get_height (fb);

  tx = W/2;
  ty = H/2;
    }

    for (i = 96; i < 1280; i+=2)
      {
        fill_render (fb, frag_ripple_interference, i);
        event_handling (fb);
      }

    ghostbuster (fb);
    fill_render (fb, frag_mandel, i);
    sleep (1);
    ghostbuster (fb);

    mmm_add_message (fb, "foo");

    for (i = 0; i < 32; i+=1)
      {
        fill_render (fb, frag_hack2, 64-i);
        event_handling (fb);
      }
    mmm_add_message (fb, "bar");



    for (i = 0; i < 32; i+=1)
      {
        fill_render (fb, frag_hack, i);
        event_handling (fb);
      }

    ghostbuster (fb);

    for (i = 0; i < 74; i+=1)
    {
        fill_render (fb, frag_ripple, i * 1000);
        event_handling (fb);
    }
    for (i = 74; i >0; i-=2)
    {
        fill_render (fb, frag_ripple, i * 1000);
        event_handling (fb);
    }
    ghostbuster (fb);

    event_handling (fb);
  }

  mmm_destroy (fb);
  fprintf (stderr, "nano-test done!\n");
  return 0;
}
