#ifndef MMM_FB_H
#define MMM_FB_H

#include <string.h>
#include <stdint.h>

typedef struct Mmm_ Mmm;
typedef struct _MmmRectangle MmmRectangle;

struct _MmmRectangle {
  int x;
  int y;
  int width;
  int height;
};

typedef enum {
  MMM_FLAG_DEFAULT = 0,
  MMM_FLAG_BUFFER  = 1 << 0  /* hint that we want an extra double buffering layer */
} MmmFlag;

/* create a new framebuffer client, passing in -1, -1 tries to request
 * a fullscreen window for "tablet" and smaller, and a 400, 300 portait
 * by default for desktops.
 */
Mmm*           mmm_new                  (int width, int height,
                                         MmmFlag flags, void *babl_format);

// XXX: desirable with unified parameter api for:
//   resizable
//   fullscreen
//   on-top
//   decorations

/* shut down an mmm.
 */
void           mmm_destroy              (Mmm *fb);
int            mmm_get_bytes_per_pixel  (Mmm *fb);

/* mmm_set_size:
 * @fb: an fdev in between reads and writes
 * @width: new width
 * @height: new height
 *
 * Resizes a buffer, the buffer is owned by the client that created it,
 * pass -1, -1 to get auto (maximized/fullscreen) dimensions.
 */
void           mmm_set_size             (Mmm *fb, int width, int height);

void           mmm_get_size             (Mmm *fb, int *width, int *height);

/* set a title to be used
 */
void           mmm_set_title            (Mmm *fb, const char *title);
const char *   mmm_get_title            (Mmm *fb);

/* modify the windows position in compositor/window-manager coordinates
 */
void           mmm_set_x                (Mmm *fb, int x);
void           mmm_set_y                (Mmm *fb, int y);
int            mmm_get_x                (Mmm *fb);
int            mmm_get_y                (Mmm *fb);

/* query the dimensions of an mmm, note that these values should not
 * be used as basis for stride/direct pixel updates, use the _get_buffer
 * functions, and the returned buffer and dimensions for that.
 */
int            mmm_get_width            (Mmm *fb);
int            mmm_get_height           (Mmm *fb);

/* Get the native babl_format of a buffer, requires babl
 * to be compiled in.
 */
const char*    mmm_get_babl_format      (Mmm *fb);

/* mmm_get_buffer_write:
 * @fb      framebuffer-client
 * @width   pointer to integer where width will be stored
 * @height  pointer to integer where height will be stored
 * @stride  pointer to integer where stride will be stored
 *
 * Get a pointer to memory when we've got data to put into it, this should be
 * called at the last possible minute, since some of the returned buffer, the
 * width, the height or stride might be invalidated after the corresponding
 * mmm_write_done() call.
 *
 * Return value: pointer to framebuffer, or return NULL if there is nowhere
 * to write data or an error.
 */
unsigned char *mmm_get_buffer_write (Mmm *fb,
                                     int *width, int *height,
                                     int *stride,
                                     void *babl_format);

/* mmm_write_done:
 * @fb: an mmm framebuffer
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
void           mmm_write_done       (Mmm *fb,
                                     int damage_x, int damage_y,
                                     int damage_width, int damage_height);

/* event queue:  */
void           mmm_add_event        (Mmm *fb, const char *event);
int            mmm_has_event        (Mmm *fb);
const char    *mmm_get_event        (Mmm *fb);

/* message queue:  */
void           mmm_add_message      (Mmm *fb, const char *message);
int            mmm_has_message      (Mmm *fb);
const char    *mmm_get_message      (Mmm *fb);

/****** the following are for use by the compositor implementation *****/

/* check for damage, returns true if there is damage/pending data to draw
 */
int            mmm_get_damage       (Mmm *fb,
                                     int *x, int *y,
                                     int *width, int *height);

/* read only access to the buffer, this is most likely a superfluous call
 * for clients themselves; it is useful for compositors.
 */
const unsigned char *mmm_get_buffer_read (Mmm *fb,
                                          int *width, int *height,
                                          int *stride);

/* this clears accumulated damage.  */
void           mmm_read_done        (Mmm *fb);

/* open up a buffer - as held by a client */
Mmm           *mmm_host_open        (const char *path);

/* check if the dimensions have changed */
int            mmm_host_check_size  (Mmm *fb, int *width, int *height);

/* and a corresponding call for the client side  */
int            mmm_client_check_size (Mmm *fb, int *width, int *height);

void           mmm_host_get_size (Mmm *fb, int *width, int *height);
void           mmm_host_set_size (Mmm *fb, int width, int height);

/* warp the _mouse_ cursor to given coordinates; doesn't do much on a
 * touch-screen
 */
void           mmm_warp_cursor (Mmm *fb, int x, int y);

/* a clock source, counting since startup in microseconds.
 */
long           mmm_ticks       (void);

long           mmm_client_pid  (Mmm *fb);

/*** audio ***/

typedef enum {
  MMM_f32,
  MMM_f32S,
  MMM_s16,
  MMM_s16S
} MmmAudioFormat;

void mmm_pcm_set_sample_rate   (Mmm *fb, int freq);
int  mmm_pcm_get_sample_rate   (Mmm *fb);
void mmm_pcm_set_format        (Mmm *fb, MmmAudioFormat format);
int  mmm_pcm_get_free_frames   (Mmm *fb);
int  mmm_pcm_get_frame_chunk   (Mmm *fb);
int  mmm_pcm_write             (Mmm *fb, const int8_t *data, int frames);
int  mmm_pcm_get_queued_frames (Mmm *fb);
int  mmm_pcm_read              (Mmm *fb, int8_t *data, int frames);
int  mmm_pcm_bpf               (Mmm *fb);

#endif
