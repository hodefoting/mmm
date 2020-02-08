#include "mmm.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "host.h"

#include <pthread.h>
#include <alsa/asoundlib.h>

#define DESIRED_PERIOD_SIZE 200

static float  host_freq   = 48000;
static MmmPCM host_format = MMM_s16S;

static snd_pcm_t *alsa_open (char *dev, int rate, int channels)
{
   snd_pcm_hw_params_t *hwp;
   snd_pcm_sw_params_t *swp;
   snd_pcm_t *h;
   int r;
   int dir;
   snd_pcm_uframes_t period_size_min;
   snd_pcm_uframes_t period_size_max;
   snd_pcm_uframes_t period_size;
   snd_pcm_uframes_t buffer_size;

   if ((r = snd_pcm_open(&h, dev, SND_PCM_STREAM_PLAYBACK, 0) < 0))
           return NULL;

   hwp = alloca(snd_pcm_hw_params_sizeof());
   memset(hwp, 0, snd_pcm_hw_params_sizeof());
   snd_pcm_hw_params_any(h, hwp);

   snd_pcm_hw_params_set_access(h, hwp, SND_PCM_ACCESS_RW_INTERLEAVED);
   snd_pcm_hw_params_set_format(h, hwp, SND_PCM_FORMAT_S16_LE);
   snd_pcm_hw_params_set_rate(h, hwp, rate, 0);
   snd_pcm_hw_params_set_channels(h, hwp, channels);

   dir = 0;
   snd_pcm_hw_params_get_period_size_min(hwp, &period_size_min, &dir);
   dir = 0;
   snd_pcm_hw_params_get_period_size_max(hwp, &period_size_max, &dir);

   period_size = DESIRED_PERIOD_SIZE;

   dir = 0;
   r = snd_pcm_hw_params_set_period_size_near(h, hwp, &period_size, &dir);
   r = snd_pcm_hw_params_get_period_size(hwp, &period_size, &dir);
   buffer_size = period_size * 4;
   r = snd_pcm_hw_params_set_buffer_size_near(h, hwp, &buffer_size);
   r = snd_pcm_hw_params(h, hwp);
   swp = alloca(snd_pcm_sw_params_sizeof());
   memset(hwp, 0, snd_pcm_sw_params_sizeof());
   snd_pcm_sw_params_current(h, swp);
   r = snd_pcm_sw_params_set_avail_min(h, swp, period_size);
   snd_pcm_sw_params_set_start_threshold(h, swp, 0);
   r = snd_pcm_sw_params(h, swp);
   r = snd_pcm_prepare(h);

   return h;
}


static  snd_pcm_t *h = NULL;
static int paused = 0;

static void *alsa_audio_start(Host *host)
{
//  Lyd *lyd = aux;
  int c;
  int16_t data[81920*4];

  /* The audio handler is implemented as a mixer that adds data on top
   * of 0s, XXX: it should be ensured that minimal work is there is
   * no data available.
   */



  for (;;)
  {
    int got_data = 0;
    int host_channels = mmm_pcm_channels (host_format);
    if (host_has_quit)
      return NULL;

    if (h)
    {
      c = snd_pcm_wait(h, 1000);

      if (c >= 0)
         c = snd_pcm_avail_update(h);

      if (c > 2000) c = 2000; // should use max mmm buffer sizes

      if (c == -EPIPE)
        snd_pcm_prepare(h);
    }
    else
    {
       c = 1000;
    }

    if (c > 0)
    {
       if (host_has_quit)
         return NULL;

       {
        MmmList *l;
        //int16_t temp_audio[81920 * 2];

        for (int i = 0; i < c * host_channels; i ++)
        {
          data[i] = 0;
        }

        for (l = host->clients; l; l = l->next)
        {
          Client *client = l->data;
          float factor = mmm_pcm_get_sample_rate (client->mmm) * 1.0 / host_freq;
          int read = 0;
          int16_t *dst = (void*) data;
          int remaining = c;
          int requested;
          MmmPCM client_format = mmm_pcm_get_format (client->mmm);

          int cbpf = mmm_pcm_bytes_per_frame (client_format);
          int hbpf = mmm_pcm_bytes_per_frame (host_format);
          int cchannels = mmm_pcm_channels (client_format);
          int hchannels = mmm_pcm_channels (host_format);
          int cbps = cbpf / cchannels;
          int hbps = hbpf / hchannels;

          int cfloat = 0;
          if (client_format == MMM_f32 ||
              client_format == MMM_f32S)
            cfloat = 1;

          if (mmm_pcm_get_queued_frames (client->mmm) >= c)
          do {
            requested = remaining;
            {
              int request = remaining * factor;
              uint8_t tempbuf[request * 8];
              read = mmm_pcm_read (client->mmm, (void*)tempbuf, request);

              if (read)
              {
                uint8_t *tdst = (void*)dst;

                if (cfloat)
                {
                  int i;
                  for (i = 0; i < read / factor; i ++)
                  {
                    int j;
                    for (j = 0; j < hchannels; j ++)
                    {
                      int cchan = j >= cchannels ? cchannels-1 : j; // mono to stereo
                      float val = *(float*)(&tempbuf[((int)(i * factor)) * cbpf + cchan * cbps]);
                      int16_t ival = val * (1<<15);
                      *(int16_t*)(&tdst[i * hbpf + j * hbps]) += ival;
                    }
                  }
                }
                else
                {
                  int i;
                  for (i = 0; i < read / factor; i ++)
                  {
                    int j;
                    for (j = 0; j < hchannels; j ++)
                    {
                      int cchan = j >= cchannels ? cchannels-1 : j; // mono to stereo
                      int16_t ival = *(int16_t*)(&tempbuf[((int)(i * factor)) * cbpf + cchan * cbps]);
                      *(int16_t*)(&tdst[i * hbpf + j * hbps]) += ival;

                    }
                  }
                }

                remaining -= read / factor;
                got_data++;
              }
            }
          } while ((read == requested) && remaining > 0);
        }
      }
/* XXX : can we turn this off when we haven't had writes for a while? to save power if possible? */
      if (got_data)
      {

        if (!h)
        {
          h = alsa_open("default", host_freq, mmm_pcm_channels (host_format));
          if (!h)
          {
            fprintf(stderr, "MMM: unable to open ALSA device (%d channels, %.0f Hz), dying\n",
            mmm_pcm_channels (host_format), host_freq);
           return 0;
          }
          snd_pcm_prepare(h);
          snd_pcm_wait(h, c);
        }
	if (paused && h)
	{
	  if (h) snd_pcm_pause (h, 0);
	  paused = 0;
	}

        c = snd_pcm_writei(h, data, c);
        if (c < 0)
          c = snd_pcm_recover (h, c, 0);
      }
      else
      {
	if (!paused)
	{
	  if (h) snd_pcm_pause (h, 1);
	  paused = 1;
	}
        // TODO : shut down audio when all clients are silent?
        usleep (20000);
      }
    } else {
      if (getenv("LYD_FATAL_UNDERRUNS"))
        {
          printf ("dying XXxx need to add API for this debug\n");
          //printf ("%i", lyd->active);
          exit(0);
        }
      if (got_data)
        fprintf (stderr, "alsa underun\n");
      //exit(0);
    }
  }
  return NULL;
}

int
audio_init_alsa (Host *host)
{
  pthread_t tid;

  pthread_create(&tid, NULL, (void*)alsa_audio_start, host);
  return 1;
}
