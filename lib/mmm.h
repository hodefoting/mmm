#ifndef UFB_FB_H
#define UFB_FB_H

#include <string.h>
#include <stdint.h>

typedef struct Ufb_ Ufb;
typedef struct _UfbRectangle UfbRectangle;

struct _UfbRectangle {
  int x;
  int y;
  int width;
  int height;
};

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

// XXX: desirable with unified parameter api for:
//   resizable
//   fullscreen
//   on-top
//   decorations

/* shut down an ufb.
 */
void           ufb_destroy              (Ufb *fb);
int            ufb_get_bytes_per_pixel  (Ufb *fb);

/* ufb_set_size:
 * @fb: an fdev in between reads and writes
 * @width: new width
 * @height: new height
 *
 * Resizes a buffer, the buffer is owned by the client that created it,
 * pass -1, -1 to get auto (maximized/fullscreen) dimensions.
 */
void           ufb_set_size             (Ufb *fb, int width, int height);

void           ufb_get_size             (Ufb *fb, int *width, int *height);

/* set a title to be used
 */
void           ufb_set_title            (Ufb *fb, const char *title);
const char *   ufb_get_title            (Ufb *fb);

/* modify the windows position in compositor/window-manager coordinates
 */
void           ufb_set_x                (Ufb *fb, int x);
void           ufb_set_y                (Ufb *fb, int y);
int            ufb_get_x                (Ufb *fb);
int            ufb_get_y                (Ufb *fb);

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

#endif
