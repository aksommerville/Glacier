#include "ma_raspi_internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>

/* Current time in microseconds.
 */
 
static int64_t ma_raspi_first_time=0;
 
int64_t ma_raspi_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  int64_t now=(int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
  if (!ma_raspi_first_time) ma_raspi_first_time=now;
  return now;
}

/* Public time functions.
 */
 
uint32_t millis() {
  return (ma_raspi_now()-ma_raspi_first_time)/1000;
}

uint32_t micros() {
  return ma_raspi_now()-ma_raspi_first_time;
}

void delay(uint32_t ms) {
  usleep(ms*1000);
}

/* Audio callback.
 */
 
static void ma_raspi_cb_audio(int16_t *v,int c,struct ma_alsa *alsa) {
  int chanc=ma_alsa_get_chanc(alsa);
  if (chanc>1) {
    int framec=c/chanc;
    for (;framec-->0;) {
      int16_t sample=audio_next();
      int i=chanc;
      for (;i-->0;v++) *v=i;
    }
  } else {
    int16_t lo=0x7fff,hi=-0x8000;
    for (;c-->0;v++) {
      *v=audio_next();
      if (*v<lo) lo=*v;
      else if (*v>hi) hi=*v;
    }
  }
}

/* Init.
 */
 
uint8_t ma_init(struct ma_init_params *params) {

  if (params) {
    params->videow=96;
    params->videoh=64;
    memcpy(&ma_raspi_init_params,params,sizeof(struct ma_init_params));
  } else {
    ma_raspi_init_params.videow=96;
    ma_raspi_init_params.videoh=64;
    ma_raspi_init_params.audio_rate=22050;
  }
  
  if (ma_raspi_init_params.audio_rate) {
    if (!(ma_alsa=ma_alsa_new(
      ma_raspi_init_params.audio_rate,1,ma_raspi_cb_audio,0
    ))) {
      fprintf(stderr,"Failed to initialize ALSA at %d Hz. Proceeding without...\n",ma_raspi_init_params.audio_rate);
      ma_raspi_init_params.audio_rate=0;
    } else {
      ma_raspi_init_params.audio_rate=ma_alsa_get_rate(ma_alsa);
    }
  }
  
  return 1;
}

/* Calculate sleep time.
 */
 
static int ma_raspi_calculate_sleep_time_us() {
  if (!ma_raspi_init_params.rate) return 0;
  int64_t now=ma_raspi_now();
  if (now<ma_raspi_next_frame_time) {
    return (ma_raspi_next_frame_time-now)&INT_MAX;
  }
  int interval=1000000/ma_raspi_init_params.rate;
  ma_raspi_next_frame_time+=interval;
  if (now>ma_raspi_next_frame_time) {
    ma_raspi_frame_skipc++;
    ma_raspi_next_frame_time=now+interval;
  }
  return (ma_raspi_next_frame_time-now)&INT_MAX;
}

/* Update.
 */
 
uint16_t ma_update() {

  if (ma_evdev) {
    ma_evdev_update(ma_evdev);
  }

  int sleepus=ma_raspi_calculate_sleep_time_us();
  if (sleepus>0) usleep(sleepus);
  
  /* Now this is weird, sorry.
   * We want to lock audio during the app's update.
   * We can't just wrap the call to loop(), since loop() calls ma_update() which is where we sleep (see just above).
   * So we lock it right here, then the outer layer, ma_raspi_update() will unlock after loop() is done.
   */
  if (ma_alsa&&!ma_raspi_audio_locked) {
    if (ma_alsa_lock(ma_alsa)>=0) ma_raspi_audio_locked=1;
  }
  
  return ma_raspi_input;
}

/* Receive framebuffer.
 */

void ma_send_framebuffer(const void *fb) {
  ma_bcm_swap(ma_bcm,fb);
}

/* Host path from TinyArcade one.
 */
 
static int ma_raspi_mangle_path(char *dst,int dsta,const char *src) {
  if (!ma_raspi_file_sandbox) return -1;
  if (!src) return -1;
  while (*src=='/') src++;
  if (!*src) return -1;
  int dstc=snprintf(dst,dsta,"%s/%s",ma_raspi_file_sandbox,src);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  return dstc;
}

/* Read file.
 */

int32_t ma_file_read(void *dst,int32_t dsta,const char *tapath,int32_t seek) {
  if (!dst||(dsta<1)) return -1;
  char path[1024];
  int pathc=ma_raspi_mangle_path(path,sizeof(path),tapath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  int fd=open(path,O_RDONLY);
  if (fd<0) return -1;
  if (seek&&(lseek(fd,seek,SEEK_SET)!=seek)) {
    close(fd);
    return -1;
  }
  int32_t dstc=0;
  while (dstc<dsta) {
    int32_t err=read(fd,dst+dstc,dsta-dstc);
    if (err<=0) break;
    dstc+=err;
  }
  close(fd);
  return dstc;
}

/* Write file.
 */
 
int32_t ma_file_write(const char *tapath,const void *src,int32_t srcc) {
  if (!src||(srcc<0)) return -1;
  char path[1024];
  int pathc=ma_raspi_mangle_path(path,sizeof(path),tapath);
  if ((pathc<1)||(pathc>=sizeof(path))) return -1;
  int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0666);
  if (fd<0) return -1;
  int32_t srcp=0;
  while (srcp<srcc) {
    int32_t err=write(fd,(char*)src+srcp,srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return -1;
    }
    srcp+=err;
  }
  close(fd);
  return 0;
}
