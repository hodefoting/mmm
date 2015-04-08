/*
 * Copyright 2008 Sean Fox <dyntryx@gmail.com>
 * Copyright 2008 James Bursa <james@netsurf-browser.org>
 *
 * This file is part of NetSurf's libnsgif, http://www.netsurf-browser.org/
 * Licenced under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 */

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "libnsgif.h"
#include "mmm.h"
#include "mmm-pset.h"

unsigned char *load_file(const char *path, size_t *data_size);
void warning(const char *context, int code);
void *bitmap_create(int width, int height);
void bitmap_set_opaque(void *bitmap, bool opaque);
bool bitmap_test_opaque(void *bitmap);
unsigned char *bitmap_get_buffer(void *bitmap);
void bitmap_destroy(void *bitmap);
void bitmap_modified(void *bitmap);

static gif_animation gif;
static unsigned char *gif_data;

int gif_decode (int frame_no, char *dst);
unsigned char *gif_anim_open (char *path, int *w, int *h, int *s)
{
  //ufb_eink_mono (fb);

	gif_bitmap_callback_vt bitmap_callbacks = {
		bitmap_create,
		bitmap_destroy,
		bitmap_get_buffer,
		bitmap_set_opaque,
		bitmap_test_opaque,
		bitmap_modified
	};
	size_t size;
	gif_result code;
	/* create our gif animation */
	gif_create(&gif, &bitmap_callbacks);
	/* load file into memory */
  gif_data = load_file(path, &size);
	/* begin decoding */
	do {
		code = gif_initialise(&gif, size, gif_data);
		if (code != GIF_OK && code != GIF_WORKING) {
			warning("gif_initialise", code);
      fprintf (stderr, "fail\n");
			exit(1);
		}
	} while (code != GIF_OK);

	printf("P3\n");
	printf("# %s\n", path);
	printf("# width                %u \n", gif.width);
	printf("# height               %u \n", gif.height);
	printf("# frame_count          %u \n", gif.frame_count);
	printf("# frame_count_partial  %u \n", gif.frame_count_partial);
	printf("# loop_count           %u \n", gif.loop_count);
	printf("%u %u 256\n", gif.width, gif.height * gif.frame_count);

  if (w) *w = gif.width;
  if (s) *s = gif.width * 4;
  if (h) *h = gif.height;

  return (unsigned char *) gif.frame_image;
}

int gif_length (void)
{
  return gif.frame_count;
}

int gif_decode (int frame_no, char *dst)
{
  int code = gif_decode_frame(&gif, frame_no);
	if (code != GIF_OK)
		warning("gif_decode_frame", code);
  if (dst)
    memcpy (dst, gif.frame_image, gif.width * gif.height * 4);
  return gif.frames[frame_no].frame_delay;
}

void gif_end (void)
{
  /* clean up */
  gif_finalise (&gif);
	free (gif_data);
  // XXX: move these back here from other marooned location in
  // code.. XXX:
  //ufb_eink_mono_end (fb);
}

int decode_gif_anim (Mmm *fb, char *path)
{
	gif_bitmap_callback_vt bitmap_callbacks = {
		bitmap_create,
		bitmap_destroy,
		bitmap_get_buffer,
		bitmap_set_opaque,
		bitmap_test_opaque,
		bitmap_modified
	};
	gif_animation gif;
	size_t size;
	gif_result code;
	unsigned int i;

	/* create our gif animation */
	gif_create(&gif, &bitmap_callbacks);

	/* load file into memory */
	unsigned char *data = load_file(path, &size);

	/* begin decoding */
	do {
		code = gif_initialise(&gif, size, data);
		if (code != GIF_OK && code != GIF_WORKING) {
			warning("gif_initialise", code);
      fprintf (stderr, "fail\n");
			exit(1);
		}
	} while (code != GIF_OK);


	/* decode the frames */
  int l;
  for (l = 0; l < 100; l++)
	for (i = 0; i != gif.frame_count; i++) {
		unsigned int row, col;
		unsigned char *image;

    unsigned char *buffer;
    unsigned char *dst;
    int width; int height; int stride;

		code = gif_decode_frame(&gif, i);
		if (code != GIF_OK)
			warning("gif_decode_frame", code);

		//printf("# frame %u:   delay: %i\n", i, gif.frames[i].frame_delay);
    usleep (gif.frames[i].frame_delay * 1000 * 10);

    mmm_client_check_size (fb, &width, &height);
    buffer = mmm_get_buffer_write (fb, &width, &height, &stride, NULL);

		image = (unsigned char *) gif.frame_image;

    int x, y;
    int ox, oy;
    int k = 0;

    float scale = 1.0 * gif.width / width;

    if (1.0 * gif.height /height > scale)
      scale = 1.0 * gif.height / height;

    ox = (width - (gif.width/scale))/2;
    oy = (height - (gif.height/scale))/2;

    for (y = 0; y < height-1; y++)
    {
      dst = buffer + stride * y;
      for (x = 0; x < width-1; x++, k++)
        {
          col = (x-ox) * scale;
          row = (y-oy) * scale;

          if (col < 0 || row < 0 || col > gif.width || row > gif.height)
            mmm_pset (fb, buffer, x, y, 255,255,255, 255);
          else
          {
				    size_t z = (row * gif.width + col) * 4;
            dst[2] = image[z];
            dst[1] = image[z+1];
            dst[0] = image[z+2];
            dst[3] = 255;
          }
          dst+=4;
        }
      mmm_write_done (fb, 0, 0, -1, -1);
    }
		}

    gif_finalise(&gif);
	  free(data);

	return 0;
}

int main(int argc, char *argv[])
{
  Mmm *fb = mmm_new (512, 384, 0, NULL);;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s image.gif\n", argv[0]);
		return 1;
	}
  return decode_gif_anim (fb, argv[1]);
}

unsigned char *load_file(const char *path, size_t *data_size)
{
	FILE *fd;
	struct stat sb;
	unsigned char *buffer;
	size_t size;
	size_t n;

	fd = fopen(path, "rb");
	if (!fd) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	if (stat(path, &sb)) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	size = sb.st_size;

	buffer = malloc(size);
	if (!buffer) {
		fprintf(stderr, "Unable to allocate %lld bytes\n",
				(long long) size);
		exit(EXIT_FAILURE);
	}

	n = fread(buffer, 1, size, fd);
	if (n != size) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	fclose(fd);

	*data_size = size;
	return buffer;
}


void warning(const char *context, gif_result code)
{
	fprintf(stderr, "%s failed: ", context);
	switch (code)
	{
	case GIF_INSUFFICIENT_FRAME_DATA:
		fprintf(stderr, "GIF_INSUFFICIENT_FRAME_DATA");
		break;
	case GIF_FRAME_DATA_ERROR:
		fprintf(stderr, "GIF_FRAME_DATA_ERROR");
		break;
	case GIF_INSUFFICIENT_DATA:
		fprintf(stderr, "GIF_INSUFFICIENT_DATA");
		break;
	case GIF_DATA_ERROR:
		fprintf(stderr, "GIF_DATA_ERROR");
		break;
	case GIF_INSUFFICIENT_MEMORY:
		fprintf(stderr, "GIF_INSUFFICIENT_MEMORY");
		break;
	default:
		fprintf(stderr, "unknown code %i", code);
		break;
	}
	fprintf(stderr, "\n");
}

void *bitmap_create(int width, int height)
{
	return calloc(width * height, 4);
}

void bitmap_set_opaque(void *bitmap, bool opaque)
{
	(void) opaque;  /* unused */
	assert(bitmap);
}

bool bitmap_test_opaque(void *bitmap)
{
	assert(bitmap);
	return false;
}

unsigned char *bitmap_get_buffer(void *bitmap)
{
	assert(bitmap);
	return bitmap;
}

void bitmap_destroy(void *bitmap)
{
	assert(bitmap);
	free(bitmap);
}

void bitmap_modified(void *bitmap)
{
	assert(bitmap);
	return;
}
