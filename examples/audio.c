#include "mmm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static long frames = 1;
static float hz    = 440;
#define VOLUME 0.3
static float volume = VOLUME;

static int sample_rate = 48000;

int main (int argc, char **argv)
{
  Mmm *mmm = mmm_new (200, 100, 0, NULL);
  mmm_pcm_set_sample_rate (mmm, sample_rate);
  mmm_pcm_set_format (mmm, MMM_f32S);
  mmm_set_title (mmm, "mmm audio experiment");
  int quit = 0;

  if (argv[1])
    hz = atof(argv[1]);

  for (int i = 0; (i < 1000000) && (!quit); i++)
  {
    int count = 8192 * 10;
    float buf[count * 2];
    int i;
    count = mmm_pcm_get_frame_chunk (mmm);

    if (count > 0)
    {
      for (i = 0; i < count; i++)
      {
        float phase;
        int   phasei;
        frames++;
        phase = frames / (1.0 * sample_rate / hz);
        phasei = phase;
        phase = phase - phasei;

        buf[i * 2]   = sin(phase * M_PI * 2) * volume;
        buf[i * 2+1] = sin(phase * M_PI * 2) * volume;
      }
      mmm_pcm_queue (mmm, (void*)buf, count);

      while (mmm_has_event (mmm))
      {
        const char *event = mmm_get_event (mmm);
        if (!strcmp (event, "q"))
          quit = 1;
        if (!strcmp (event, "up"))   // octave up
          hz *= 2;
        if (!strcmp (event, "down")) // octave down
          hz /= 2;
        if (!strcmp (event, "space")) // toggle tone
          volume = volume == 0.0f ? VOLUME : 0.0f;
      }
    }
    else
    {
      usleep (1000);
    }
  }
  mmm_destroy (mmm);
  return 0;
}
