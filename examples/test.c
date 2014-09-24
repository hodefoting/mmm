/* small test showing a basic example of driving a framebuffer
 */
#include "mmm.h"
#include <stdio.h>

int main ()
{
  int frame = 1024;
  Mmm *fb = mmm_new (800, 600, 0, NULL);

  if (!fb)
    {
      fprintf (stderr, "failed to open buffer\n");
      return -1;
    }

  while (frame < 16000)
  {
    int x, y;
    uint8_t *buffer;
    int width, height, stride;

    mmm_client_check_size (fb, &width, &height); /* does the real resize as a side-effect */

    buffer = mmm_get_buffer_write (fb, &width, &height, &stride, NULL);

    for (y = 0; y < height; y++)
    {
      uint8_t *pixel = &buffer[y * stride];
      for (x = 0; x < width; x++, pixel+=4)
      {
        pixel[0] = ( ( (int)(((x) / (frame/18123.0)) + (int)(y / (frame/18123.0)) )) % 2 ==  0) ? 255 : 0;
        pixel[1] = ( ( (int)(((x) / (frame/17111.0)) + (int)(y / (frame/13111.0)) )) % 2 ==  0) ? 255 : 0;
        pixel[2] = ( ( (int)(((x) / (frame/11111.0)) + (int)(y / (frame/11111.0)) )) % 2 ==  0) ? 255 : 0;
        pixel[3] = 255;
      }
    }
    mmm_write_done (fb, 0, 0, -1, -1);
    frame++;
  }

  mmm_destroy (fb);
  return 0;
}
