#include "ma_macos_internal.h"
#include <OpenGL/gl.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>

/* Globals.
 */

int16_t sinTable_q15[512];
struct ma_init_params ma_macos_init_params={0};
uint16_t ma_macos_input_state=0;
static GLuint texid=0;
static uint32_t *ma_macos_fb=0;

static struct ma_macos_inmap {
  int devid;
  int srcbtnid;
  int srcv;
  int srclo,srchi;
  int dstv;
  int dstbtnid;
} *ma_macos_inmapv=0;
static int ma_macos_inmapc=0;
static int ma_macos_inmapa=0;

/* Input map.
 */

static int ma_macos_inmap_search(int devid,int btnid) {
  int lo=0,hi=ma_macos_inmapc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (ma_macos_inmapv[ck].devid<devid) hi=ck;
    else if (ma_macos_inmapv[ck].devid>devid) lo=ck+1;
    else if (ma_macos_inmapv[ck].srcbtnid<btnid) hi=ck;
    else if (ma_macos_inmapv[ck].srcbtnid>btnid) lo=ck+1;
    else {
      while ((ck>lo)&&(ma_macos_inmapv[ck-1].devid==devid)&&(ma_macos_inmapv[ck-1].srcbtnid==btnid)) ck--;
      return ck;
    }
  }
  return -lo-1;
}

static int ma_macos_inmap_add(int devid,int srcbtnid,int srclo,int srchi,int dstbtnid) {
  int p=ma_macos_inmap_search(devid,srcbtnid);
  if (p<0) p=-p-1;
  while ((p<ma_macos_inmapc)&&(ma_macos_inmapv[p].devid==devid)&&(ma_macos_inmapv[p].srcbtnid==srcbtnid)) p++;
  if (ma_macos_inmapc>=ma_macos_inmapa) {
    int na=ma_macos_inmapa+16;
    if (na>INT_MAX/sizeof(struct ma_macos_inmap)) return -1;
    void *nv=realloc(ma_macos_inmapv,sizeof(struct ma_macos_inmap)*na);
    if (!nv) return -1;
    ma_macos_inmapv=nv;
    ma_macos_inmapa=na;
  }
  struct ma_macos_inmap *map=ma_macos_inmapv+p;
  memmove(map+1,map,sizeof(struct ma_macos_inmap)*(ma_macos_inmapc-p));
  ma_macos_inmapc++;
  map->devid=devid;
  map->srcbtnid=srcbtnid;
  map->srcv=0;
  map->srclo=srclo;
  map->srchi=srchi;
  map->dstv=0;
  map->dstbtnid=dstbtnid;
  return 0;
}

static int ma_macos_inmap_remove_device(int devid) {
  int pa=ma_macos_inmap_search(devid,0);
  if (pa<0) pa=-pa-1;
  int pz=ma_macos_inmap_search(devid+1,0);
  if (pz<0) pz=-pz-1;
  int rmc=pz-pa;
  if (rmc>0) {
    ma_macos_inmapc-=rmc;
    struct ma_macos_inmap *map=ma_macos_inmapv+pa;
    memmove(map,map+rmc,sizeof(struct ma_macos_inmap)*(ma_macos_inmapc-pa));
  }
  ma_macos_input_state=0; // just to be on the safe side
  return 0;
}

/* Keyboard event.
 */

static int cb_key(int usage,int value) {
  switch (usage) {
    #define INKEY(_usage,tag) \
      case 0x000700##_usage: if (value) ma_macos_input_state|=MA_BUTTON_##tag; else ma_macos_input_state&=~MA_BUTTON_##tag; break;
    INKEY(52,UP) // arrows...
    INKEY(51,DOWN)
    INKEY(50,LEFT)
    INKEY(4f,RIGHT)
    INKEY(1d,A) // z
    INKEY(1b,B) // x
    #undef INKEY
    //default: if (value) fprintf(stderr,"KEY 0x%08x\n",usage);
  }
  return 0;
}

/* HID event.
 */

static int cb_hid_connect(int devid) {

  int vid=ps_machid_dev_get_vendor_id(devid);
  int pid=ps_machid_dev_get_product_id(devid);
  const char *vname=ps_machid_dev_get_manufacturer_name(devid);
  const char *pname=ps_machid_dev_get_product_name(devid);
  //fprintf(stderr,"%s %d %04x:%04x '%s' '%s'\n",__func__,devid,vid,pid,vname,pname);

  // Hard-coded config for my devices... (XXX not goal state)
  switch ((vid<<16)|pid) {
    case 0x20d6ca6d: { // 'BDA' 'Pro Ex'; PS4 knockoff
        if (ma_macos_inmap_add(devid,15,-1,-1,MA_BUTTON_LEFT)<0) return -1;
        if (ma_macos_inmap_add(devid,15,1,1,MA_BUTTON_RIGHT)<0) return -1;
        if (ma_macos_inmap_add(devid,53,-1,-1,MA_BUTTON_UP)<0) return -1;
        if (ma_macos_inmap_add(devid,53,1,1,MA_BUTTON_DOWN)<0) return -1;
        if (ma_macos_inmap_add(devid,3,1,1,MA_BUTTON_A)<0) return -1;
        if (ma_macos_inmap_add(devid,2,1,1,MA_BUTTON_B)<0) return -1;
      } return 0;
  }

  int twostatenext=MA_BUTTON_A;
  int axisnext=MA_BUTTON_LEFT;
  int btnix=0;
  for (;;btnix++) {
    int btnid,usage,lo,hi,value;
    if (ps_machid_dev_get_button_info(&btnid,&usage,&lo,&hi,&value,devid,btnix)<0) break;
    //fprintf(stderr,"  [%d] #%d 0x%08x %d..%d =%d\n",btnix,btnid,usage,lo,hi,value);
    if ((lo==0)&&(hi==1)) {
      if (ma_macos_inmap_add(devid,btnid,1,1,twostatenext)<0) return -1;
      if (twostatenext==MA_BUTTON_A) twostatenext=MA_BUTTON_B; else twostatenext=MA_BUTTON_A;
    } else if ((hi-lo>=3)&&(value>lo)&&(value<hi)) {
      int mid=(hi+lo)>>1;
      int midlo=(lo+mid)>>1;
      int midhi=(hi+mid)>>1;
      if (midlo>=mid) midlo--;
      if (midhi<=mid) midhi++;
      if (axisnext==MA_BUTTON_LEFT) {
        if (ma_macos_inmap_add(devid,btnid,INT_MIN,midlo,MA_BUTTON_LEFT)<0) return -1;
        if (ma_macos_inmap_add(devid,btnid,midhi,INT_MAX,MA_BUTTON_RIGHT)<0) return -1;
        axisnext=MA_BUTTON_UP;
      } else {
        if (ma_macos_inmap_add(devid,btnid,INT_MIN,midlo,MA_BUTTON_UP)<0) return -1;
        if (ma_macos_inmap_add(devid,btnid,midhi,INT_MAX,MA_BUTTON_DOWN)<0) return -1;
        axisnext=MA_BUTTON_LEFT;
      }
    }
  }
  return 0;
}

static int cb_hid_disconnect(int devid) {
  ma_macos_inmap_remove_device(devid);
  return 0;
}

static int cb_hid_button(int devid,int btnid,int value) {
  //if (value==1) fprintf(stderr,"btnid %d\n",btnid);
  int mapp=ma_macos_inmap_search(devid,btnid);
  if (mapp<0) return 0;
  struct ma_macos_inmap *map=ma_macos_inmapv+mapp;
  for (;(mapp<ma_macos_inmapc)&&(map->devid==devid)&&(map->srcbtnid==btnid);mapp++,map++) {
    if (value==map->srcv) continue;
    map->srcv=value;
    int dstv=((value>=map->srclo)&&(value<=map->srchi))?1:0;
    if (dstv==map->dstv) continue;
    map->dstv=dstv;
    if (dstv) ma_macos_input_state|=map->dstbtnid;
    else ma_macos_input_state&=~map->dstbtnid;
  }
  return 0;
}

/* PCM callback.
 */

static void cb_pcm(int16_t *v,int c) {
  for (;c-->0;v++) *v=audio_next();
}

/* Initialize.
 * Client app calls this from setup().
 * Nonzero on success.
 */
 
uint8_t ma_init(struct ma_init_params *params) {

  // Set the IoC update rate based on requested video rate. Must be at least 1.
  memcpy(&ma_macos_init_params,params,sizeof(struct ma_init_params));
  if (ma_macos_init_params.rate) {
    ma_macioc_micros_per_frame=1000000/ma_macos_init_params.rate;
  }
  if (ma_macioc_micros_per_frame<1) ma_macioc_micros_per_frame=1;

  // Populate the sine table.
  int16_t *dst=sinTable_q15;
  int i=512;
  double p=0.0,d=(M_PI*2.0)/512.0;
  for (;i-->0;dst++,p+=d) *dst=sin(p)*32760.0;

  // Bring video online, create a window.
  const int video_scale=8;
  int winw=params->videow*video_scale;
  int winh=params->videoh*video_scale;
  if (ps_macwm_init(winw,winh,0,"GlacierRacer",cb_key)<0) return 0;
  ps_macwm_show_cursor(0);

  if (!(ma_macos_fb=malloc(params->videow*params->videoh*4))) return 0;

  glGenTextures(1,&texid);
  glBindTexture(GL_TEXTURE_2D,texid);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

  // Bring audio online if requested.
  if (params->audio_rate) {
    if (akmacaudio_init(params->audio_rate,1,cb_pcm)<0) {
      fprintf(stderr,"Failed to initialize audio (rate=%d), proceeding without.\n",params->audio_rate);
    }
  }

  // Bring HID online.
  struct ps_machid_delegate hiddelegate={
    .connect=cb_hid_connect,
    .disconnect=cb_hid_disconnect,
    .button=cb_hid_button,
  };
  if (ps_machid_init(&hiddelegate)<0) {
    fprintf(stderr,"Failed to initialize HID (joysticks), proceeding without.\n");
  }
  
  return 1;
}

/* Update.
 */

uint16_t ma_update() {
  ps_machid_update();
  return ma_macos_input_state;
}

/* Set viewport, and clear screen if it's not the whole window.
 */

static void set_viewport(int winw,int winh,int srcw,int srch) {
  if (!winw||!winh) return;
  int dsth;
  int dstw=(srcw*winh)/srch;
  if (dstw<=winw) {
    dsth=winh;
  } else {
    dstw=winw;
    dsth=(winw*srch)/srcw;
  }
  int dstx=(winw>>1)-(dstw>>1);
  int dsty=(winh>>1)-(dsth>>1);
  if ((dstx>0)||(dsty>0)||(dstx+dstw<winw)||(dsty+dsth<winh)) {
    glViewport(0,0,winw,winh);
    glClear(GL_COLOR_BUFFER_BIT);
  }
  glViewport(dstx,dsty,dstw,dsth);
}

/* Receive framebuffer.
 */

void ma_send_framebuffer(const void *fb) {

  int winw=96,winh=64;
  ps_macwm_get_size(&winw,&winh);
  set_viewport(winw,winh,ma_macos_init_params.videow,ma_macos_init_params.videoh);

  const uint8_t *src=fb;
  uint32_t *dst=ma_macos_fb;
  int i=ma_macos_init_params.videow*ma_macos_init_params.videoh;
  for (;i-->0;src++,dst++) {
    const uint8_t *rgb=ma_ctab8+(*src)*3;
    *dst=0xff000000|(rgb[2]<<16)|(rgb[1]<<8)|rgb[0];
  }

  glLoadIdentity();
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D,texid);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,ma_macos_init_params.videow,ma_macos_init_params.videoh,0,GL_RGBA,GL_UNSIGNED_BYTE,ma_macos_fb);
  glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2i(0,1); glVertex2i(-1,-1);
    glTexCoord2i(0,0); glVertex2i(-1, 1);
    glTexCoord2i(1,1); glVertex2i( 1,-1);
    glTexCoord2i(1,0); glVertex2i( 1, 1);
  glEnd();
  
  ps_macwm_flush_video();
}

/* Read or write file.
 */

struct ma_file_context {
  char *dst;
  int dsta;
  const char *src;
  int srcc;
  int seek;
  int result;
};

static int cb_read(const char *path,void *userdata) {
  struct ma_file_context *context=userdata;
  int fd=open(path,O_RDONLY);
  if (fd<0) return 0;
  if (context->seek&&(lseek(fd,context->seek,SEEK_SET)!=context->seek)) {
    close(fd);
    return 0;
  }
  int32_t dstc=0;
  while (dstc<context->dsta) {
    int32_t err=read(fd,context->dst+dstc,context->dsta-dstc);
    if (err<=0) break;
    dstc+=err;
  }
  close(fd);
  context->result=dstc;
  return 1;
}

static int cb_write(const char *path,void *userdata) {
  struct ma_file_context *context=userdata;
  int fd=open(path,O_WRONLY|O_CREAT,0666);
  if (fd<0) return 0;
  
  if (context->seek) {
    int p=lseek(fd,context->seek,SEEK_SET);
    if (p<0) {
      close(fd);
      unlink(path);
      return 0;
    }
    while (p<context->seek) {
      char buf[256]={0};
      int c=context->seek-p;
      if (c>sizeof(buf)) c=sizeof(buf);
      if (write(fd,buf,c)!=c) {
        close(fd);
        unlink(path);
        return 0;
      }
      p+=c;
    }
  }
  
  int32_t srcp=0;
  while (srcp<context->srcc) {
    int32_t err=write(fd,context->src+srcp,context->srcc-srcp);
    if (err<=0) {
      close(fd);
      unlink(path);
      return 0;
    }
    srcp+=err;
  }
  close(fd);
  context->result=srcp;
  return 1;
}

int32_t ma_file_read(void *dst,int32_t dsta,const char *path,int32_t seek) {
  if (!dst||(dsta<0)) return -1;
  struct ma_file_context context={
    .dst=dst,
    .dsta=dsta,
    .seek=seek,
    .result=-1,
  };
  if (ma_macos_mangle_path(path,cb_read,&context)<0) return -1;
  return context.result;
}

int32_t ma_file_write(const char *path,const void *src,int32_t srcc,int32_t seek) {
  if (!path||!path[0]||(srcc<0)||(srcc&&!src)) return -1;
  struct ma_file_context context={
    .src=src,
    .srcc=srcc,
    .seek=seek,
    .result=-1,
  };
  if (ma_macos_mangle_path(path,cb_write,&context)<0) return -1;
  return context.result;
}

/* Arduino timing.
 */

static int ma_macos_sec_zero=0;

uint32_t millis() {
  return micros()/1000;
}

uint32_t micros() {
  struct timeval tv={0};
  gettimeofday(&tv,0);
  //TODO Seconds will overflow every 4000 seconds or so -- a bit over an hour. Confirm that that's OK.
  if (!ma_macos_sec_zero) ma_macos_sec_zero=tv.tv_sec;
  tv.tv_sec-=ma_macos_sec_zero;
  return tv.tv_sec*1000000+tv.tv_usec;
}

void delay(uint32_t ms) {
  // Not going to worry about overflow; asking to sleep more than an hour (4G us) is crazy to begin with.
  usleep(ms*1000);
}
