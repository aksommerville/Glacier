#include "ma_drm_internal.h"
#include <endian.h>
#include <pthread.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

/* Type definition.
 */
 
struct ma_pulse {
  void (*cb)(int16_t *v,int c,struct ma_pulse *pulse);
  int rate,chanc;
  void *userdata;
  
  pa_simple *pa;
  
  pthread_t iothd;
  pthread_mutex_t iomtx;
  int ioerror;
  int iocancel; // pa_simple doesn't like regular thread cancellation
  
  int16_t *buf;
  int bufa;
};

/* I/O thread.
 */
 
static void *ma_pulse_iothd(void *arg) {
  struct ma_pulse *pulse=arg;
  while (1) {
    if (pulse->iocancel) return 0;
    
    // Fill buffer while holding the lock.
    if (pthread_mutex_lock(&pulse->iomtx)) {
      pulse->ioerror=-1;
      return 0;
    }
    pulse->cb(pulse->buf,pulse->bufa,pulse);
    if (pthread_mutex_unlock(&pulse->iomtx)) {
      pulse->ioerror=-1;
      return 0;
    }
    if (pulse->iocancel) return 0;
    
    // Deliver to PulseAudio.
    int err=0,result;
    result=pa_simple_write(pulse->pa,pulse->buf,sizeof(int16_t)*pulse->bufa,&err);
    if (pulse->iocancel) return 0;
    if (result<0) {
      pulse->ioerror=-1;
      return 0;
    }
  }
}

/* Terminate I/O thread.
 */
 
static void ma_pulse_abort_io(struct ma_pulse *pulse) {
  if (!pulse->iothd) return;
  pulse->iocancel=1;
  pthread_join(pulse->iothd,0);
  pulse->iothd=0;
}

/* Cleanup.
 */
 
void ma_pulse_del(struct ma_pulse *pulse) {
  if (!pulse) return;
  
  ma_pulse_abort_io(pulse);
  pthread_mutex_destroy(&pulse->iomtx);
  if (pulse->pa) {
    pa_simple_free(pulse->pa);
  }
  
  free(pulse);
}

/* Init PulseAudio client.
 */
 
static int ma_pulse_init_pa(struct ma_pulse *pulse) {
  int err;

  pa_sample_spec sample_spec={
    #if BYTE_ORDER==BIG_ENDIAN
      .format=PA_SAMPLE_S16BE,
    #else
      .format=PA_SAMPLE_S16LE,
    #endif
    .rate=pulse->rate,
    .channels=pulse->chanc,
  };
  int bufframec=pulse->rate/20; //TODO more sophisticated buffer length decision
  if (bufframec<20) bufframec=20;
  pa_buffer_attr buffer_attr={
    .maxlength=pulse->chanc*sizeof(int16_t)*bufframec,
    .tlength=pulse->chanc*sizeof(int16_t)*bufframec,
    .prebuf=0xffffffff,
    .minreq=0xffffffff,
  };
  
  if (!(pulse->pa=pa_simple_new(
    0, // server name
    MA_APP_NAME, // our name
    PA_STREAM_PLAYBACK,
    0, // sink name (?)
    MA_APP_NAME, // stream (as opposed to client) name
    &sample_spec,
    0, // channel map
    &buffer_attr,
    &err
  ))) {
    return -1;
  }
  
  pulse->rate=sample_spec.rate;
  pulse->chanc=sample_spec.channels;
  
  return 0;
}

/* With the final rate and channel count settled, calculate a good buffer size and allocate it.
 */
 
static int ma_pulse_init_buffer(struct ma_pulse *pulse) {

  const double buflen_target_s= 0.010; // about 100 Hz
  const int buflen_min=           128; // but in no case smaller than N samples
  const int buflen_max=         16384; // ...nor larger
  
  // Initial guess and clamp to the hard boundaries.
  pulse->bufa=buflen_target_s*pulse->rate*pulse->chanc;
  if (pulse->bufa<buflen_min) {
    pulse->bufa=buflen_min;
  } else if (pulse->bufa>buflen_max) {
    pulse->bufa=buflen_max;
  }
  // Reduce to next multiple of channel count.
  pulse->bufa-=pulse->bufa%pulse->chanc;
  
  if (!(pulse->buf=malloc(sizeof(int16_t)*pulse->bufa))) {
    return -1;
  }
  
  return 0;
}

/* Init thread.
 */
 
static int ma_pulse_init_thread(struct ma_pulse *pulse) {
  int err;
  if (err=pthread_mutex_init(&pulse->iomtx,0)) return -1;
  if (err=pthread_create(&pulse->iothd,0,ma_pulse_iothd,pulse)) return -1;
  return 0;
}

/* New client.
 */
 
struct ma_pulse *ma_pulse_new(
  int rate,int chanc,
  void (*cb)(int16_t *dst,int dstc,struct ma_pulse *pulse),
  void *userdata
) {
  
  struct ma_pulse *pulse=calloc(1,sizeof(struct ma_pulse));
  if (!pulse) return 0;
  
  pulse->rate=rate;
  pulse->chanc=chanc;
  pulse->cb=cb;

  if (ma_pulse_init_pa(pulse)<0) return 0;
  if (ma_pulse_init_buffer(pulse)<0) return 0;
  if (ma_pulse_init_thread(pulse)<0) return 0;
  
  return pulse;
}

/* Trivial accessors.
 */
 
int ma_pulse_get_rate(const struct ma_pulse *pulse) {
  return pulse->rate;
}

int ma_pulse_get_chanc(const struct ma_pulse *pulse) {
  return pulse->chanc;
}

void *ma_pulse_get_userdata(const struct ma_pulse *pulse) {
  return pulse->userdata;
}
  
int ma_pulse_get_status(const struct ma_pulse *pulse) {
  if (!pulse) return -1;
  if (pulse->ioerror) return -1;
  return 0;
}

/* Lock.
 */
 
int ma_pulse_lock(struct ma_pulse *pulse) {
  if (pthread_mutex_lock(&pulse->iomtx)) return -1;
  return 0;
}

int ma_pulse_unlock(struct ma_pulse *pulse) {
  if (pthread_mutex_unlock(&pulse->iomtx)) return -1;
  return 0;
}
