/* this is to serve as a minimal - no dependencies application
 * integrating with an mmm compositor - this example is minimal
 * enough that it doesn't even rely on the mmm .c file
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define WIDTH               512
#define HEIGHT              384
#define BPP                 4

#define MMM_PID             0x18
#define MMM_TITLE           0xb0
#define MMM_WIDTH           0x5b0
#define MMM_HEIGHT          0x5b4

#define MMM_DESIRED_WIDTH   0x5d8
#define MMM_DESIRED_HEIGHT  0x5dc


#define MMM_STRIDE          0x5b8
#define MMM_FB_OFFSET       0x5bc
#define MMM_FLIP_STATE      0x5c0

#define MMM_SIZE            0x48c70
#define MMM_FLIP_INIT       0
#define MMM_FLIP_NEUTRAL    1
#define MMM_FLIP_DRAWING    2
#define MMM_FLIP_WAIT_FLIP  3
#define MMM_FLIP_FLIPPING   4

#define PEEK(addr)     (*(int32_t*)(&ram_base[addr]))
#define POKE(addr,val) do{(*(int32_t*)(&ram_base[addr]))=(val); }while(0)

static uint8_t *ram_base  = NULL;

static uint8_t *pico_fb (int width, int height)
{
  char path[512];
  int fd;
  int size = MMM_SIZE + width * height * BPP;
  const char *mmm_path = getenv ("MMM_PATH");
  if (!mmm_path)
    return NULL;
  sprintf (path, "%s/fb.XXXXXX", mmm_path);
  fd = mkstemp (path);
  pwrite (fd, "", 1, size);
  fsync (fd);
  chmod (path, 511);
  ram_base = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  memset (ram_base, 0, size);
  strcpy ((void*)ram_base + MMM_TITLE, "foo");

  POKE (MMM_FLIP_STATE,  MMM_FLIP_INIT);
  POKE (MMM_PID,         (int)getpid());

  POKE (MMM_WIDTH,          width);
  POKE (MMM_HEIGHT,         height);
  POKE (MMM_DESIRED_WIDTH,  width);
  POKE (MMM_DESIRED_HEIGHT, height);
  POKE (MMM_STRIDE,         width * BPP);
  POKE (MMM_FB_OFFSET,      MMM_SIZE);
  POKE (MMM_FLIP_STATE,     MMM_FLIP_NEUTRAL);

  return (uint8_t*) & PEEK(MMM_SIZE);
}

static void    pico_exit (void)
{
  if (ram_base)
    munmap (ram_base, MMM_SIZE * PEEK (MMM_WIDTH) * PEEK (MMM_HEIGHT) * BPP);
  ram_base = NULL;
}

static void wait_sync (void)
{
  /* this client will block if there is no compositor */
  while (PEEK(MMM_FLIP_STATE) != MMM_FLIP_NEUTRAL)
    usleep (5000);
  POKE (MMM_FLIP_STATE, MMM_FLIP_DRAWING);
}

static void flip_buffer (void)
{
  POKE (MMM_FLIP_STATE, MMM_FLIP_WAIT_FLIP);
}

int main (int argc, char **argv)
{
  uint8_t *pixels = pico_fb (512, 384);

  if (!pixels)
    return -1;

  int frame;
  int val = 0;
  int dir = 1;

  for (frame = 0; frame < 100000; frame ++)
  {
    int x, y;
    int i;

    wait_sync ();

    i = 0;
    for (y = 0 ; y < HEIGHT; y ++)
      for (x = 0; x < WIDTH; x ++)
      {
        float d = (x - WIDTH/2) * (x - WIDTH / 2) +
                  (y - HEIGHT/2) * (y - HEIGHT / 2);
        pixels[i + 0] = (x & y) + frame;
        pixels[i + 1] = val;
        pixels[i + 2] = (x - WIDTH/2) * frame / ((y-HEIGHT/2)+0.5);
        pixels[i + 3] = d * 800 / frame;
        i += 4;
      }

    flip_buffer ();

    val += dir;
    if (val >= 255 || val < 0)
    {
      dir *= -1;
      val += dir;
    }
  }

  pico_exit ();
  return 0;
}
