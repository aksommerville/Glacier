#include "ma_raspi_internal.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#define ALSA_BUFFER_SIZE 2048

/* Instance
 */

struct ma_alsa {
  int rate,chanc;
  void (*cb)(int16_t *dst,int dstc,struct ma_alsa *alsa);
  void *userdata;

  snd_pcm_t *alsa;
  snd_pcm_hw_params_t *hwparams;

  int hwbuffersize;
  int bufc; // frames
  int bufc_samples;
  int16_t *buf;

  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioabort;
  int cberror;
};

/* Cleanup.
 */

void ma_alsa_del(struct ma_alsa *alsa) {
  if (!alsa) return;
  alsa->ioabort=1;
  if (alsa->iothd&&!alsa->cberror) {
    pthread_cancel(alsa->iothd);
    pthread_join(alsa->iothd,0);
  }
  pthread_mutex_destroy(&alsa->iomtx);
  if (alsa->hwparams) snd_pcm_hw_params_free(alsa->hwparams);
  if (alsa->alsa) snd_pcm_close(alsa->alsa);
  if (alsa->buf) free(alsa->buf);
  free(alsa);
}

/* I/O thread.
 */

static void *ma_alsa_iothd(void *dummy) {
  struct ma_alsa *alsa=dummy;
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,0);
  while (1) {
    pthread_testcancel();

    if (pthread_mutex_lock(&alsa->iomtx)) {
      alsa->cberror=1;
      return 0;
    }
    alsa->cb(alsa->buf,alsa->bufc_samples,alsa);
    pthread_mutex_unlock(&alsa->iomtx);

    int16_t *samplev=alsa->buf;
    int samplep=0,samplec=alsa->bufc;
    while (samplep<samplec) {
      pthread_testcancel();
      int err=snd_pcm_writei(alsa->alsa,samplev+samplep,samplec-samplep);
      if (alsa->ioabort) return 0;
      if (err<=0) {
        if ((err=snd_pcm_recover(alsa->alsa,err,0))<0) {
          alsa->cberror=1;
          return 0;
        }
        break;
      }
      samplep+=err;
    }
  }
  return 0;
}

/* Init.
 */

static int ma_alsa_init(struct ma_alsa *alsa) {
  
  if (
    (snd_pcm_open(&alsa->alsa,"default",SND_PCM_STREAM_PLAYBACK,0)<0)||
    (snd_pcm_hw_params_malloc(&alsa->hwparams)<0)||
    (snd_pcm_hw_params_any(alsa->alsa,alsa->hwparams)<0)||
    (snd_pcm_hw_params_set_access(alsa->alsa,alsa->hwparams,SND_PCM_ACCESS_RW_INTERLEAVED)<0)||
    (snd_pcm_hw_params_set_format(alsa->alsa,alsa->hwparams,SND_PCM_FORMAT_S16)<0)||
    (snd_pcm_hw_params_set_rate_near(alsa->alsa,alsa->hwparams,&alsa->rate,0)<0)||
    (snd_pcm_hw_params_set_channels(alsa->alsa,alsa->hwparams,alsa->chanc)<0)||
    (snd_pcm_hw_params_set_buffer_size(alsa->alsa,alsa->hwparams,ALSA_BUFFER_SIZE)<0)||
    (snd_pcm_hw_params(alsa->alsa,alsa->hwparams)<0)
  ) return -1;

  if (snd_pcm_nonblock(alsa->alsa,0)<0) return -1;
  if (snd_pcm_prepare(alsa->alsa)<0) return -1;

  alsa->bufc=ALSA_BUFFER_SIZE;
  alsa->bufc_samples=alsa->bufc*alsa->chanc;
  if (!(alsa->buf=malloc(alsa->bufc_samples*sizeof(int16_t)))) return -1;

  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_settype(&mattr,PTHREAD_MUTEX_RECURSIVE);
  if (pthread_mutex_init(&alsa->iomtx,&mattr)) return -1;
  pthread_mutexattr_destroy(&mattr);
  if (pthread_create(&alsa->iothd,0,ma_alsa_iothd,alsa)) return -1;

  return 0;
}

struct ma_alsa *ma_alsa_new(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc,struct ma_alsa *alsa),
  void *userdata
) {
  struct ma_alsa *alsa=calloc(1,sizeof(struct ma_alsa));
  if (!alsa) return 0;

  alsa->rate=rate;
  alsa->chanc=chanc;
  alsa->cb=cb;
  alsa->userdata=userdata;

  if (ma_alsa_init(alsa)<0) {
    ma_alsa_del(alsa);
    return 0;
  }
  return alsa;
}

/* Locks and maintenance.
 */
 
int ma_alsa_get_rate(const struct ma_alsa *alsa) {
  return alsa->rate;
}

int ma_alsa_get_chanc(const struct ma_alsa *alsa) {
  return alsa->chanc;
}

void *ma_alsa_get_userdata(const struct ma_alsa *alsa) {
  return alsa->userdata;
}

int ma_alsa_update(struct ma_alsa *alsa) {
  if (alsa->cberror) return -1;
  return 0;
}

int ma_alsa_lock(struct ma_alsa *alsa) {
  if (pthread_mutex_lock(&alsa->iomtx)) return -1;
  return 0;
}

int ma_alsa_unlock(struct ma_alsa *alsa) {
  pthread_mutex_unlock(&alsa->iomtx);
  return 0;
}
