
#include "mmm.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static long frames = 1;
static float hz    = 440;

int main (int argc, char **argv)
{
  Mmm *mmm = mmm_new (300, 300, 0, NULL);
  mmm_pcm_set_sample_rate (mmm, 44100);
  mmm_pcm_set_format (mmm, MMM_f32S);
  int quit = 0;

  if (argv[1])
    hz = atof(argv[1]);

  for (int i = 0; (i < 100) && (!quit); i++)
  {
    int count = 8192 * 10;
    float buf[count * 2];
    int i;
    count = mmm_pcm_get_frame_chunk (mmm);

    if (count <= 0)
      continue;

    for (i = 0; i < count; i++)
    {
      float phase;
      int   phasei;
      frames++;
      phase = frames / (44100.0 / hz);
      phasei = phase;
      phase = phase - phasei;

      buf[i * 2]   = sin(phase * M_PI * 2) * 0.5;
      buf[i * 2+1] = sin(phase * M_PI * 2) * 0.5;
    }
    mmm_pcm_write (mmm, (void*)buf, count);

    while (mmm_has_event (mmm))
    {
      const char *event = mmm_get_event (mmm);
      if (!strcmp (event, "q"))
        quit = 1;
    }
  }

  mmm_destroy (mmm);
  return 0;
}
