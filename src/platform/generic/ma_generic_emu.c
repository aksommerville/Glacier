#include "ma_generic_internal.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/poll.h>

/* Current time in microseconds.
 */
 
static int64_t ma_generic_first_time=0;
 
int64_t ma_generic_now() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  int64_t now=(int64_t)tv.tv_sec*1000000ll+tv.tv_usec;
  if (!ma_generic_first_time) ma_generic_first_time=now;
  return now;
}

/* Public time functions.
 */
 
uint32_t millis() {
  return (ma_generic_now()-ma_generic_first_time)/1000;
}

uint32_t micros() {
  return ma_generic_now()-ma_generic_first_time;
}

void delay(uint32_t ms) {
  usleep(ms*1000);
}

/* Init.
 */
 
uint8_t ma_init(struct ma_init_params *params) {
  if (params) {
    params->videow=96;
    params->videoh=64;
    params->audio_rate=0;
    memcpy(&ma_generic_init_params,params,sizeof(struct ma_init_params));
  } else {
    ma_generic_init_params.videow=96;
    ma_generic_init_params.videoh=64;
    ma_generic_init_params.audio_rate=0;
  }
  return 1;
}

/* Calculate sleep time.
 */
 
static int ma_generic_calculate_sleep_time_us() {
  if (!ma_generic_init_params.rate) return 0;
  int64_t now=ma_generic_now();
  if (now<ma_generic_next_frame_time) {
    return (ma_generic_next_frame_time-now)&INT_MAX;
  }
  int interval=1000000/ma_generic_init_params.rate;
  ma_generic_next_frame_time+=interval;
  if (now>ma_generic_next_frame_time) {
    ma_generic_frame_skipc++;
    ma_generic_next_frame_time=now+interval;
  }
  return (ma_generic_next_frame_time-now)&INT_MAX;
}

/* Read input.
 */
 
static int ma_generic_read_stdin_1(const char *src,int srcc) {

  /* This 'ma_generic_input_next' was an attempt to catch quick re-presses of a button.
   * Unfortunately, we already have to defeat the host's autorepeat, and I'm not sure how to do both.
   * So we're only using 'keyhold' -- when we get an ON event, make it linger for a fixed time.
   * User goes off and on faster than that interval, we don't notice the OFF.
   * It's not ideal, but, well, you're using a TTY as a video game platform, so... ok.
   */

  switch (src[0]) {
    #define KEY(src,tag,khix,rtn) case src: { \
        ma_generic_input|=MA_BUTTON_##tag; \
        ma_generic_keyhold[khix]=ma_generic_keyhold_time; \
      } return rtn;
    case 0x04: return 0; // EOF in noncanonical mode
    case 0x1b: { // Escapes (eg arrow keys)
        if ((srcc>=3)&&(src[1]==0x5b)) {
          switch (src[2]) {
            KEY(0x41,UP,2,3)
            KEY(0x42,DOWN,3,3)
            KEY(0x43,RIGHT,1,3)
            KEY(0x44,LEFT,0,3)
          }
          return 3;
        }
        if ((srcc<2)||(src[1]!=0x5b)) return 0; // actual escape key
      } return 1;
    KEY('z',A,4,1)
    KEY('x',B,5,1)
    #undef KEY
  }
  return 1;
}
 
static void ma_generic_read_stdin() {
  char src[256];
  int srcc=read(STDIN_FILENO,src,sizeof(src));
  if (srcc<=0) {
    ma_generic_quit_requested=1;
    return;
  }
  int srcp=0;
  while (srcp<srcc) {
    int err=ma_generic_read_stdin_1(src+srcp,srcc-srcp);
    if (err<1) {
      ma_generic_quit_requested=1;
      return;
    }
    srcp+=err;
  }
}

/* Update.
 */
 
uint16_t ma_update() {
  int sleepus=ma_generic_calculate_sleep_time_us();
  
  uint16_t pvinput=ma_generic_input;
  ma_generic_input=ma_generic_input_next;
  ma_generic_input_next=0;

  if (ma_generic_use_stdin) {
    struct pollfd pollfd={
      .fd=STDIN_FILENO,
      .events=POLLIN|POLLERR|POLLHUP,
    };
    if (poll(&pollfd,1,sleepus/1000)>0) {
      if (pollfd.revents&(POLLERR|POLLHUP)) {
        ma_generic_quit_requested=1;
      } else if (pollfd.revents&POLLIN) {
        ma_generic_read_stdin();
      }
    }
    if (sleepus) sleepus=ma_generic_calculate_sleep_time_us();
  }
  
  if (sleepus) usleep(sleepus);
  
  if (ma_generic_keyhold[0]&&!(ma_generic_input_next&MA_BUTTON_LEFT)) { ma_generic_keyhold[0]--; ma_generic_input|=MA_BUTTON_LEFT; }
  if (ma_generic_keyhold[1]&&!(ma_generic_input_next&MA_BUTTON_RIGHT)) { ma_generic_keyhold[1]--; ma_generic_input|=MA_BUTTON_RIGHT; }
  if (ma_generic_keyhold[2]&&!(ma_generic_input_next&MA_BUTTON_UP)) { ma_generic_keyhold[2]--; ma_generic_input|=MA_BUTTON_UP; }
  if (ma_generic_keyhold[3]&&!(ma_generic_input_next&MA_BUTTON_DOWN)) { ma_generic_keyhold[3]--; ma_generic_input|=MA_BUTTON_DOWN; }
  if (ma_generic_keyhold[4]&&!(ma_generic_input_next&MA_BUTTON_A)) { ma_generic_keyhold[4]--; ma_generic_input|=MA_BUTTON_A; }
  if (ma_generic_keyhold[5]&&!(ma_generic_input_next&MA_BUTTON_B)) { ma_generic_keyhold[5]--; ma_generic_input|=MA_BUTTON_B; }
  
  return ma_generic_input;
}

/* Receive framebuffer.
 */
 
static int ma_generic_tty_color_from_tiny(ma_pixel_t src) {
  // First normalize to 8-bit channels.
  #if MA_PIXELSIZE==8
    uint8_t b=src&0xe0; b|=b>>3; b|=b>>6;
    uint8_t g=src&0x1c; g|=g<<3; g|=g>>6;
    uint8_t r=src&0x03; r|=r<<2; r|=r<<4;
  #else
    uint8_t b=(src&0xf800)>>8; b|=b>>5;
    uint8_t g=(src&0x07e0)>>3; g|=g>>6;
    uint8_t r=(src&0x001f)<<3; r|=r>>5;
  #endif
      
  // Grays are 232..255 = black..white
  int color;
  if ((b==g)&&(g==r)) {
    color=232+(b*24)/255;
    if (color<232) color=232;
    else if (color>255) color=255;
  // Ignore the first 16. Everything else is a 6x6x6 cube.
  } else {
    r=(r*6)>>8; if (r>5) r=5;
    g=(g*6)>>8; if (g>5) g=5;
    b=(b*6)>>8; if (b>5) b=5;
    color=16+r*36+g*6+b;
  }

  return color;
}
 
static char ma_ttytext[80000];

void ma_send_framebuffer(const void *fb) {
  if (ma_generic_use_stdout) {
    char *text=ma_ttytext;
    int textc=0;
    const ma_pixel_t *src=fb;
    char *dst=text;
    memcpy(dst,"\x1b[H",3); dst+=3; textc+=3;
    int yi=ma_generic_init_params.videoh>>1;
    for (;yi-->0;) {
      int xi=ma_generic_init_params.videow;
      for (;xi-->0;src++) {
        int fgcolor=ma_generic_tty_color_from_tiny(src[0]);
        int bgcolor=ma_generic_tty_color_from_tiny(src[ma_generic_init_params.videow]);
        // U+2580 fg on top
        // 0010 0101 1000 0000
        // 11100010 10010110 10000000
        // e2 96 80
        int err=snprintf(
          dst,sizeof(ma_ttytext)-textc,
          "\x1b[38;5;%dm\x1b[48;5;%dm\xe2\x96\x80",//25
          fgcolor,bgcolor
        );
        dst+=err;
        textc+=err;
      }
      memcpy(dst,"\x1b[0m\n",5); dst+=5; textc+=5;
      src+=ma_generic_init_params.videow; // skip a row
    }
    write(STDOUT_FILENO,text,textc);
  }
}

/* Host path from TinyArcade one.
 */
 
static int ma_generic_mangle_path(char *dst,int dsta,const char *src) {
  if (!ma_generic_file_sandbox) return -1;
  if (!src) return -1;
  while (*src=='/') src++;
  if (!*src) return -1;
  int dstc=snprintf(dst,dsta,"%s/%s",ma_generic_file_sandbox,src);
  if ((dstc<1)||(dstc>=dsta)) return -1;
  return dstc;
}

/* Read file.
 */

int32_t ma_file_read(void *dst,int32_t dsta,const char *tapath,int32_t seek) {
  if (!dst||(dsta<1)) return -1;
  char path[1024];
  int pathc=ma_generic_mangle_path(path,sizeof(path),tapath);
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
  int pathc=ma_generic_mangle_path(path,sizeof(path),tapath);
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
