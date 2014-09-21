/* this is to serve as a minimal - no dependencies application
 * integrating with an ufb compositor - this example is minimal
 * enough that it doesn't even rely on the ufb .c file
 */



#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define WIDTH              512
#define HEIGHT             384
#define BPP                4

#define UFB_PID             0x18
#define UFB_WIDTH           0x3b0
#define UFB_HEIGHT          0x3b4
#define UFB_STRIDE          0x3b8
#define UFB_FB_OFFSET       0x3bc
#define UFB_FLIP_STATE      0x3c0
//#define UFB_SIZE            131124  // too low and it will conflict with pcm-data

#define UFB_SIZE 0x41548
#define UFB_FLIP_INIT       0
#define UFB_FLIP_NEUTRAL    1
#define UFB_FLIP_DRAWING    2
#define UFB_FLIP_WAIT_FLIP  3
#define UFB_FLIP_FLIPPING   4

#define PEEK(addr)     (*(int32_t*)(&ram_base[addr]))
#define POKE(addr,val) do{(*(int32_t*)(&ram_base[addr]))=(val); }while(0)

static uint8_t *ram_base  = NULL;

static uint8_t *pico_fb (int width, int height)
{
  char path[512];
  int fd;
  int size = UFB_SIZE + width * height * BPP;
  const char *ufb_path = getenv ("UFB_PATH");
  if (!ufb_path)
    return NULL;
  sprintf (path, "%s/fb.XXXXXX", ufb_path);
  fd = mkstemp (path);
  pwrite (fd, "", 1, size);
  fsync (fd);
  chmod (path, 511);
  ram_base = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  POKE (UFB_FLIP_STATE,  UFB_FLIP_INIT);
  POKE (UFB_PID,         (int)getpid());
  POKE (UFB_WIDTH,       width);
  POKE (UFB_HEIGHT,      height);
  POKE (UFB_STRIDE,      width * BPP);
  /* format? */
  POKE (UFB_FB_OFFSET,   UFB_SIZE);
  POKE (UFB_FLIP_STATE,  UFB_FLIP_NEUTRAL);

#if 0
  poke to start pcm

  poke to start events
#endif



  return (uint8_t*) & PEEK(UFB_SIZE);
}

static void    pico_exit (void)
{
  if (ram_base)
    munmap (ram_base, UFB_SIZE * PEEK (UFB_WIDTH) * PEEK (UFB_HEIGHT) * BPP);
  ram_base = NULL;
}

static void wait_sync (void)
{
  /* this client will block if there is no compositor */
  while (PEEK(UFB_FLIP_STATE) != UFB_FLIP_NEUTRAL)
    usleep (5000);
  POKE (UFB_FLIP_STATE, UFB_FLIP_DRAWING);
}

static void flip_buffer (void)
{
  POKE (UFB_FLIP_STATE, UFB_FLIP_WAIT_FLIP);
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
