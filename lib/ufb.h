#ifndef UFB_FB_H
#define UFB_FB_H

#include <string.h>
#include <stdint.h>

typedef struct Ufb_ Ufb;

typedef enum {
  UFB_FLAG_DEFAULT = 0,
  UFB_FLAG_BUFFER  = 1 << 0  /* hint that we want an extra double buffering layer */
} UfbFlag;

/* create a new framebuffer client, passing in -1, -1 tries to request
 * a fullscreen window for "tablet" and smaller, and a 400, 300 portait
 * by default for desktops.
 */
Ufb*           ufb_new                  (int width, int height,
                                         UfbFlag flags, void *babl_format);

/* shut down an ufb.
 */
void           ufb_destroy              (Ufb *fb);

/* ufb_get_bytes_per_pixel:
 * @fb: an ufb
 * 
 * Name says it all.
 */
int            ufb_get_bytes_per_pixel  (Ufb *fb);

/* ufb_set_size:
 * @fb: an fdev in between reads and writes
 * @width: new width
 * @height: new height
 *
 * Resizes a buffer, the buffer is owned by the client that created it,
 * pass -1, -1 to get auto (maximized) dimensions.
 */
void           ufb_set_size             (Ufb *fb, int width, int height);

void           ufb_get_size             (Ufb *fb, int *width, int *height);

/* set a title to be used
 */
void           ufb_set_title            (Ufb *fb, const char *title);
const char *   ufb_get_title            (Ufb *fb);

/* modify the windows position in compositor/window-manager coordinates
 */
void           ufb_set_x (Ufb *fb, int x);
void           ufb_set_y (Ufb *fb, int y);
int            ufb_get_x (Ufb *fb);
int            ufb_get_y (Ufb *fb);

/* ufb_set_fps_limit:
 * @fb an ufb framebuffer
 * @fps_limit new fps limit.
 *
 * Enables an internal frame-limiter for /dev/fb use - that sleeps for the
 * time remaining for drawing a full frame. If set to 0 this rate limiter
 * is not enabled.
 */
void           ufb_set_fps_limit        (Ufb *fb, int fps_limit);

/* query the dimensions of an ufb, note that these values should not
 * be used as basis for stride/direct pixel updates, use the _get_buffer
 * functions, and the returned buffer and dimensions for that.
 */
int            ufb_get_width            (Ufb *fb);
int            ufb_get_height           (Ufb *fb);

/* Get the native babl_format of a buffer, requires babl
 * to be compiled in.
 */
const char*    ufb_get_babl_format      (Ufb *fb);

/* ufb_get_buffer_write:
 * @fb      framebuffer-client
 * @width   pointer to integer where width will be stored
 * @height  pointer to integer where height will be stored
 * @stride  pointer to integer where stride will be stored
 *
 * Get a pointer to memory when we've got data to put into it, this should be
 * called at the last possible minute, since some of the returned buffer, the
 * width, the height or stride might be invalidated after the corresponding
 * ufb_write_done() call.
 *
 * Return value: pointer to framebuffer, or return NULL if there is nowhere
 * to write data or an error.
 */
unsigned char *ufb_get_buffer_write (Ufb *fb,
                                     int *width, int *height,
                                     int *stride,
                                     void *babl_format);

/* ufb_write_done:
 * @fb: an ufb framebuffer
 * @damage_x: upper left most pixel damaged
 * @damage_y: upper left most pixel damaged
 * @damage_width: width bound of damaged pixels, or -1 for all
 * @damage_height: height bound of damaged pixels, or -1 for all
 *
 * Reports that writing to the buffer is complete, if possible provide the
 * bounding box of the changed region - as eink devices; as well as
 * compositing window managers want to know this information to do efficient
 * updates. width/height of -1, -1 reports that any pixel in the buffer might
 * have changed.
 */
void           ufb_write_done       (Ufb *fb,
                                     int damage_x, int damage_y,
                                     int damage_width, int damage_height);

/* event queue:  */
void           ufb_add_event        (Ufb *fb, const char *event);
int            ufb_has_event        (Ufb *fb);
const char    *ufb_get_event        (Ufb *fb);

/* message queue:  */
void           ufb_add_message      (Ufb *fb, const char *message);
int            ufb_has_message      (Ufb *fb);
const char    *ufb_get_message      (Ufb *fb);

/****** the following are for use by the compositor implementation *****/

/* check for damage, returns true if there is damage/pending data to draw
 */
int            ufb_get_damage       (Ufb *fb,
                                     int *x, int *y,
                                     int *width, int *height);

/* read only access to the buffer, this is most likely a superfluous call
 * for clients themselves; it is useful for compositors.
 */
const unsigned char *ufb_get_buffer_read (Ufb *fb,
                                          int *width, int *height,
                                          int *stride);

/* this clears accumulated damage.  */
void           ufb_read_done        (Ufb *fb);

/* open up a buffer - as held by a client */
Ufb           *ufb_host_open        (const char *path);

/* check if the dimensions have changed */
int            ufb_host_check_size  (Ufb *fb, int *width, int *height);

/* and a corresponding call for the client side  */
int            ufb_client_check_size (Ufb *fb, int *width, int *height);

void           ufb_host_get_size (Ufb *fb, int *width, int *height);
void           ufb_host_set_size (Ufb *fb, int width, int height);

/* warp the _mouse_ cursor to given coordinates; doesn't do much on a
 * touch-screen
 */
void           ufb_warp_cursor (Ufb *fb, int x, int y);

/* a clock source, counting since startup in microseconds.
 */
long           ufb_ticks       (void);

long           ufb_client_pid  (Ufb *fb);

/*** audio ***/

typedef enum {
  UFB_f32,
  UFB_f32S,
  UFB_s16,
  UFB_s16S
} UfbAudioFormat;


void ufb_pcm_set_sample_rate   (Ufb *fb, int freq);
int  ufb_pcm_get_sample_rate   (Ufb *fb);
void ufb_pcm_set_format        (Ufb *fb, UfbAudioFormat format);
int  ufb_pcm_get_free_frames   (Ufb *fb);
int  ufb_pcm_get_frame_chunk   (Ufb *fb);
int  ufb_pcm_write             (Ufb *fb, const int8_t *data, int frames);
int  ufb_pcm_get_queued_frames (Ufb *fb);
int  ufb_pcm_read              (Ufb *fb, int8_t *data, int frames);
int  ufb_pcm_bpf               (Ufb *fb);


/* pset */

#ifndef PSET_H
#define PSET_H

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

static inline void ufb_dither_mono (int x, int y, 
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

static inline void ufb_dither_rgb (int x, int y, 
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

static inline void ufb_dither_generic (int x, int y, 
                                       int *red,
                                       int *green,
                                       int *blue)
{
  if (eink_is_mono)
    ufb_dither_mono (x, y, red, green, blue);
  else
    ufb_dither_rgb (x, y, red, green, blue);
}

#undef CLAMP

/* somewhat optimized pset routines; as well as unwrapped version that
 * permits efficient looping.
 */

#define Yu8_SET(p,r,g,b,a) do{p[0] = ((r)+(g)+(b))/3;}while(0)
#define RGBu565_SET(p,r,g,b,a) do{p[0] = (*((uint16_t*)(p)) = ((r) >> 3) + (((g)>>2) << 5) + ((b>> 3) << (5+6)));}while(0);
#define RGBu8_SET(p,r,g,b,a) do{ p[0] = (r); p[1] = (g); p[2] = (b); }while(0)
#define RGBAu8_SET(p,r,g,b,a) do{ p[0] = (r); p[1] = (g); p[2] = (b); p[3] = (a); }while(0)
  

inline static unsigned char *ufb_pix_pset_nodither  (
    Ufb *fb, unsigned char *pix, int bpp, int x, int y,
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

inline static unsigned char *ufb_pix_pset_mono  (
    Ufb *fb, unsigned char *pix, int bpp, int x, int y,
    int red, int green, int blue, int alpha)
{
  int mono = (red+green+blue)/3;
  ufb_dither_mono (x, y, &mono, &mono, &mono);

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

#define UFB_PCM_BUFFER_SIZE  8192

#include <stdio.h>

inline static unsigned char *ufb_pix_pset (
    Ufb *fb, unsigned char *pix, int bpp, int x, int y,
    int red, int green, int blue, int alpha)
{
  //fprintf (stderr, "%i %i %i %i\n", x, y, ufb_get_width (fb), ufb_get_height (fb));
  ufb_dither_rgb (x, y, &red, &green , &blue);

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

inline static unsigned char *ufb_get_pix (Ufb *fb, unsigned char *buffer, int x, int y)
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
inline static void ufb_pset (Ufb *fb, unsigned char *buffer, int x, int y, int red, int green, int blue, int alpha)
{
  int bpp =    ((int*)(fb))[0];
  unsigned char *pix = ufb_get_pix (fb, buffer, x, y);
  ufb_pix_pset (fb, pix, bpp, x, y, red, green, blue, alpha);
}

inline static void ufb_pset_mono (Ufb *fb, unsigned char *buffer, int x, int y, int red, int green, int blue, int alpha)
{
  int bpp =    ((int*)(fb))[0];
  unsigned char *pix = ufb_get_pix (fb, buffer, x, y);
  ufb_pix_pset_mono (fb, pix, bpp, x, y, red, green, blue, alpha);
}

#endif

/********/


#endif
