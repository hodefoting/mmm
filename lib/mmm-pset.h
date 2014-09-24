/* pset */

#ifndef MMFB_PSET_H
#define MMFB_PSET_H

#include <math.h>

 /* dithered optimized, partial accessible due to inlined factorization
  */

#ifndef  CLAMP
#define CLAMP(num, min, max) {if (num < min) num = min;if (num > max)num=max;}
#endif

extern int eink_is_mono;

static inline int a_dither_trc (int input)
{
  static int inited = 0;
  static int trc[1024];
  if (!inited)
  {
    int i;
    inited = 1;
    for (i = 0; i< 1024; i++)
      trc[i] = round(pow (i / 1023.0, 0.75) * 1023.999);
  }
  if (input < 0) input = 0;
  if (input > 1023) input = 1023;
  return trc[input];
}

static inline int a_dither(int x, int y)
{
  return ((x ^ y * 237) * 181 & 511)/2.00;
  //return ((x+    (y+(y<<2)+(y<<3)+(y<<8)))*3213) & 0xff;
}

static inline void mmm_dither_mono (int x, int y, 
                                    int *red,
                                    int *green,
                                    int *blue)
{
  int dither = a_dither(x,y);
  int value = a_dither_trc (*red + *green * 2.5 + *blue / 2) ;
  value = value <= dither ? 0 : 255;
  if(red)*red = value;
  if (red == green) /* quick bail, when red==green==blue it indicates gray*/
                    /* scale conversion,
                     * XXX: need to special case this for this case.. `*/
    return;
  if(green)*green = value;
  if(blue)*blue = value; 
}

static inline void mmm_dither_rgb (int x, int y, 
                                   int *red,
                                   int *green,
                                   int *blue)
{
  int dither = a_dither(x,y);
  dither = (dither >> 3) - 16;
  *red = a_dither_trc (*red * 4) + dither;
  CLAMP(*red, 0, 255);

  //*red = a_dither(x,y);
  if (red == green) /* quick bail, when red==green==blue it indicates gray*/
                    /* scale conversion */
    return;

  *green = a_dither_trc (*green * 4) + dither;
  CLAMP(*green, 0, 255);
  *blue = a_dither_trc (*blue * 4) + dither;
  CLAMP(*blue, 0, 255);
}

static inline void mmm_dither_generic (int x, int y, 
                                       int *red,
                                       int *green,
                                       int *blue)
{
  if (eink_is_mono)
    mmm_dither_mono (x, y, red, green, blue);
  else
    mmm_dither_rgb (x, y, red, green, blue);
}

#undef CLAMP

/* somewhat optimized pset routines; as well as unwrapped version that
 * permits efficient looping.
 */

#define Yu8_SET(p,r,g,b,a) do{p[0] = ((r)+(g)+(b))/3;}while(0)
#define RGBu565_SET(p,r,g,b,a) do{p[0] = (*((uint16_t*)(p)) = ((r) >> 3) + (((g)>>2) << 5) + ((b>> 3) << (5+6)));}while(0);
#define RGBu8_SET(p,r,g,b,a) do{ p[0] = (r); p[1] = (g); p[2] = (b); }while(0)
#define RGBAu8_SET(p,r,g,b,a) do{ p[0] = (r); p[1] = (g); p[2] = (b); p[3] = (a); }while(0)
  

inline static unsigned char *mmm_pix_pset_nodither  (
    Mmm *fb, unsigned char *pix, int bpp, int x, int y,
    int red, int green, int blue, int alpha)
{
  switch (bpp)
  {
    case 1: /* grayscale framebuffer */
      Yu8_SET(pix,red,green,blue,alpha);
      break;
    case 2: /* 16bit frame buffer, R' u5 G' u6 B' u5 */
      RGBu565_SET(pix,red,green,blue,alpha);
      break;
    case 3: /* 24bit frame buffer, R'G'B' u8 */
      RGBu8_SET(pix,red,green,blue,alpha);
      break;
    case 4: /* 32bit frame buffer, R'G'B'A u8 */
      RGBAu8_SET(pix,red,green,blue,alpha);
      break;
  }
  return pix + bpp;
}

inline static unsigned char *mmm_pix_pset_mono  (
    Mmm *fb, unsigned char *pix, int bpp, int x, int y,
    int red, int green, int blue, int alpha)
{
  int mono = (red+green+blue)/3;
  mmm_dither_mono (x, y, &mono, &mono, &mono);

  switch (bpp)
  {
    case 1: /* grayscale framebuffer */
      Yu8_SET(pix,mono,mono,mono,alpha);
      break;
    case 2: /* 16bit frame buffer, R' u5 G' u6 B' u5 */
      RGBu565_SET(pix,mono,mono,mono,alpha);
      break;
    case 3: /* 24bit frame buffer, R'G'B' u8 */
      RGBu8_SET(pix,mono,mono,mono,alpha);
      break;
    case 4: /* 32bit frame buffer, R'G'B'A u8 */
      RGBAu8_SET(pix,mono,mono,mono,alpha);
      break;
  }
  return pix + bpp;
}


#include <stdio.h>

inline static unsigned char *mmm_pix_pset (
    Mmm *fb, unsigned char *pix, int bpp, int x, int y,
    int red, int green, int blue, int alpha)
{
  //fprintf (stderr, "%i %i %i %i\n", x, y, mmm_get_width (fb), mmm_get_height (fb));
  //mmm_dither_rgb (x, y, &red, &green , &blue);

  switch (bpp)
  {
    case 1: /* grayscale framebuffer */
      Yu8_SET(pix,red,green,blue,alpha);
      break;
    case 2: /* 16bit frame buffer, R' u5 G' u6 B' u5 */
      RGBu565_SET(pix,red,green,blue,alpha);
      break;
    case 3: /* 24bit frame buffer, R'G'B' u8 */
      RGBu8_SET(pix,red,green,blue,alpha);
      break;
    case 4: /* 32bit frame buffer, R'G'B'A u8 */
      RGBAu8_SET(pix,red,green,blue,alpha);
      break;
  }
  return pix + bpp;
}

inline static unsigned char *mmm_get_pix (Mmm *fb, unsigned char *buffer, int x, int y)
{
  /* this works, because bpp is the first member of fb, quickly getting this
   * in a generic function like 
  this is worth the hack. */
  int bpp =    ((int*)(fb))[0];
  int stride = ((int*)(fb))[1];
  unsigned char *pix = &buffer[y * stride + x * bpp];
  return pix;
}

/* a sufficiently fast pixel setter
 */
inline static void mmm_pset (Mmm *fb, unsigned char *buffer, int x, int y, int red, int green, int blue, int alpha)
{
  int bpp =    ((int*)(fb))[0];
  unsigned char *pix = mmm_get_pix (fb, buffer, x, y);
  mmm_pix_pset (fb, pix, bpp, x, y, red, green, blue, alpha);
}

inline static void mmm_pset_mono (Mmm *fb, unsigned char *buffer, int x, int y, int red, int green, int blue, int alpha)
{
  int bpp =    ((int*)(fb))[0];
  unsigned char *pix = mmm_get_pix (fb, buffer, x, y);
  mmm_pix_pset_mono (fb, pix, bpp, x, y, red, green, blue, alpha);
}

#endif

/********/
