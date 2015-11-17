


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <alsa/asoundlib.h>

#include "log.h"
#include "alsa.h"



int alsa_device_open( const char *device_name, struct alsa_config *config,
        snd_pcm_t **capture_handle, snd_pcm_t **playback_handle )
{
    snd_pcm_hw_params_t *hw_params = NULL;
    snd_pcm_sw_params_t *sw_params = NULL;

    if (capture_handle) *capture_handle = NULL;
    if (playback_handle) *playback_handle = NULL;

    snd_pcm_uframes_t period_size = config->period;
    int period_count = config->buffer_period_count;
    snd_pcm_uframes_t buffer_size = period_count * period_size;
    int dir, r;

    if (capture_handle) {
        /* open the capture */

        if ((r = snd_pcm_open (capture_handle, device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
           err( "%s c: cannot open audio device(%s)", device_name, snd_strerror (r));
           *capture_handle = NULL;
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
           err("%s c: cannot allocate hardware parameter structure (%s)", device_name, snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_any (*capture_handle, hw_params)) < 0) {
           err("%s c: cannot initialize hardware parameter structure (%s)", device_name, snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_access (*capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
           err("%s c: cannot set access type (%s)", device_name, snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_format (*capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
           err("%s c: cannot set sample format (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_rate_near (*capture_handle, hw_params, &config->rate, 0)) < 0) {
           err("%s c: cannot set sample rate (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_channels (*capture_handle, hw_params, config->channels)) < 0) {
           err("%s c: cannot set channel count (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }

        dir = 0;
        if ((r = snd_pcm_hw_params_set_period_size_near (*capture_handle, hw_params, &period_size, &dir)) < 0) {
           err("%s c: cannot set period size (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }
        if (period_size != config->period) {
            warn("%s c: period size %u can't be used. set to %u instead",device_name, config->period, (unsigned)period_size  );
            config->period = period_size;
        }
        buffer_size = period_size * period_count;
        dir=0;
        if ((r = snd_pcm_hw_params_set_buffer_size_near (*capture_handle, hw_params, &buffer_size)) < 0) {
           err("%s c: cannot set buffer time (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params (*capture_handle, hw_params)) < 0) {
           err("%s c: cannot set capture parameters (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
               err("%s c: cannot allocate software parameters structure (%s)", device_name, snd_strerror (r));
               goto open_failed;
            }
        if ((r = snd_pcm_sw_params_current (*capture_handle, sw_params)) < 0) {
           err("%s c: cannot initialize software parameters structure (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }
        if ((r = snd_pcm_sw_params_set_avail_min (*capture_handle, sw_params, period_size)) < 0) {
           err("%s c: cannot set minimum available count (%s)", device_name ,snd_strerror (r));
           goto open_failed;
        }
        /*
        if ((r = snd_pcm_sw_params_set_start_threshold (*capture_handle, sw_params, period)) < 0) {
           err("cannot set start mode (%s)",snd_strerror (r));
           goto open_failed;
        }
        */
        if ((r = snd_pcm_sw_params (*capture_handle, sw_params)) < 0) {
           err("%s c: cannot set software parameters (%s)", device_name,snd_strerror (r));
           goto open_failed;
        }

        snd_pcm_hw_params_free(hw_params);
        snd_pcm_sw_params_free(sw_params);
        hw_params = NULL;
        sw_params = NULL;

    }

    if (playback_handle) {
        if ((r = snd_pcm_open (playback_handle, device_name, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
           err("%s p: cannot open audio device (%s)",device_name,snd_strerror (r));
           *playback_handle = NULL;
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
           err("%s p: cannot allocate hardware parameter structure (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_any (*playback_handle, hw_params)) < 0) {
           err("%s p: cannot initialize hardware parameter structure (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_access (*playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
           err("%s p: cannot set access type (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_format (*playback_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
           err("%s p: cannot set sample format (%s)",device_name, snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_rate_near (*playback_handle, hw_params, &config->rate, 0)) < 0) {
           err("%s p: cannot set sample rate (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }

        if ((r = snd_pcm_hw_params_set_channels (*playback_handle, hw_params, config->channels)) < 0) {
           err("%s p: cannot set channel count (%s)",device_name, snd_strerror (r));
           goto open_failed;
        }

        dir = 0;
        dbg("set period size: %d", (int)period_size);
        if ((r = snd_pcm_hw_params_set_period_size_near (*playback_handle, hw_params, &period_size, &dir)) < 0) {
           err("%s p: cannot set period size (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }
        if (period_size != config->period) {
             warn("%s p: period size %d can't be used. set to %d instead",device_name, config->period, (int)period_size );
             config->period = period_size;
        }
        /*
        if ((r = snd_pcm_hw_params_set_periods (*playback_handle, hw_params, 2, 0)) < 0) {
           err("%s p: cannot set number of periods (%s)",device_name, snd_strerror (r));
           goto open_failed;
        }
        */
        buffer_size = period_size * period_count;
        dir=0;
        if ((r = snd_pcm_hw_params_set_buffer_size_near (*playback_handle, hw_params, &buffer_size)) < 0) {
           err("%s p: cannot set buffer time (%s)",device_name, snd_strerror (r));
           goto open_failed;
        }


        if ((r = snd_pcm_hw_params (*playback_handle, hw_params)) < 0) {
           err("%s p: cannot set playback parameters (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }

        /*snd_pcm_dump_setup(dev->playback_handle, jcd_out);*/


        if ((r = snd_pcm_sw_params_malloc (&sw_params)) < 0) {
           err("%s p: cannot allocate software parameters structure (%s)",device_name ,snd_strerror (r));
           goto open_failed;
        }
        if ((r = snd_pcm_sw_params_current (*playback_handle, sw_params)) < 0) {
           err("%s p: cannot initialize software parameters structure (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }
        if ((r = snd_pcm_sw_params_set_avail_min (*playback_handle, sw_params, period_size)) < 0) {
           err("%s p: cannot set minimum available count (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }
        if ((r = snd_pcm_sw_params_set_start_threshold (*playback_handle, sw_params, (period_count -1) * period_size)) < 0) {
           err("%s p: cannot set start mode (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }
        if ((r = snd_pcm_sw_params (*playback_handle, sw_params)) < 0) {
           err("%s p: cannot set software parameters (%s)",device_name,snd_strerror (r));
           goto open_failed;
        }
        snd_pcm_hw_params_free(hw_params);
        snd_pcm_sw_params_free(sw_params);
        hw_params = NULL;
        sw_params = NULL;
    }

    if (capture_handle && playback_handle && config->linking_capture_playback) {
        if ((r = snd_pcm_link(*capture_handle, *playback_handle) != 0)) {
            err("snd_pcm_link not possible for ALSA device %s: %s",
                    device_name,
                    snd_strerror (r)
                    );
        }
    }
    return 0;

open_failed:
    if (hw_params) snd_pcm_hw_params_free(hw_params);
    if (sw_params) snd_pcm_sw_params_free(sw_params);
    if (capture_handle && *capture_handle) {
        snd_pcm_close(*capture_handle);
        *capture_handle = NULL;
    }
    if (playback_handle && *playback_handle) {
        snd_pcm_close(*playback_handle);
        *playback_handle = NULL;
    }
    return -1;
}