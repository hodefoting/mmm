/* this is to serve as a minimal - no dependencies application
 * integrating with an ufb compositor - this example is minimal
 * enough that it doesn't even rely on the ufb .c file
 */

#include "../lib/mmm.c"

int main ()
{
  UfbShm test;
#define GET_ADDR(a) \
  fprintf (stderr, "%s: %p\n", #a, (void*)((uint8_t*)& (test.a) - (uint8_t*)&test));
  
  GET_ADDR(header.pid)
  GET_ADDR(fb.width)
  GET_ADDR(fb.height)
  GET_ADDR(fb.stride)
  GET_ADDR(fb.fb_offset)
  GET_ADDR(fb.flip_state)

  GET_ADDR(fb.x)
  GET_ADDR(fb.y)
  GET_ADDR(fb.damage_x)
  GET_ADDR(fb.damage_y)
  GET_ADDR(fb.damage_width)
  GET_ADDR(fb.damage_height)

  GET_ADDR(pcm.format)
  GET_ADDR(pcm.sample_rate)
  GET_ADDR(pcm.write)
  GET_ADDR(pcm.read)
  GET_ADDR(pcm.buffer)

  fprintf (stderr, "%p\n", (void*)sizeof (UfbShm));

  return 0;
}
