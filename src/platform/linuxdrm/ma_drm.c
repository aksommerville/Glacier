#include "ma_drm_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// At 13, it fills my monitor but also runs very slow.
// Kind of surprising. I guess if we want to use libdrm for reals, we do need GL.
#define MA_DRM_SCALE_LIMIT 6

#if MA_PIXELSIZE==8
  typedef uint32_t ma_drm_pixel_t;
#else
  typedef uint16_t ma_drm_pixel_t;
#endif

/* Instance definition.
 */

struct ma_drm {
  int fd;
  
  uint32_t connid,encid,crtcid;
  drmModeCrtcPtr crtc_restore;
  drmModeModeInfo mode;
  
  int fbw,fbh; // input size, established at init (expect 96x64)
  int screenw,screenh; // screen size, also size of our framebuffers
  int dstx,dsty,dstw,dsth; // scale target within (screen)
  int scale; // 1..MA_DRM_SCALE_LIMIT; fbw*scale=dstw<=screenw
  
  struct ma_drm_fb {
    uint32_t fbid;
    int handle;
    int size;
    void *v;
  } fbv[2];
  int fbp; // (0,1) which is attached -- draw to the other
  
  #if MA_PIXELSIZE==8
    uint32_t ctab[256];
  #endif
};

/* Cleanup.
 */
 
static void ma_drm_fb_cleanup(struct ma_drm *drm,struct ma_drm_fb *fb) {
  if (fb->fbid) {
    drmModeRmFB(drm->fd,fb->fbid);
  }
  if (fb->v) {
    munmap(fb->v,fb->size);
  }
}
 
void ma_drm_del(struct ma_drm *drm) {
  if (!drm) return;
  
  ma_drm_fb_cleanup(drm,drm->fbv+0);
  ma_drm_fb_cleanup(drm,drm->fbv+1);

  if (drm->crtc_restore) {
    if (drm->fd>=0) {
      drmModeCrtcPtr crtc=drm->crtc_restore;
      drmModeSetCrtc(
        drm->fd,
        crtc->crtc_id,
        crtc->buffer_id,
        crtc->x,
        crtc->y,
        &drm->connid,
        1,
        &crtc->mode
      );
    }
    drmModeFreeCrtc(drm->crtc_restore);
  }

  if (drm->fd>=0) {
    close(drm->fd);
    drm->fd=-1;
  }
  
  free(drm);
}

/* Connector and mode chosen.
 */
 
static int ma_drm_init_with_conn(
  struct ma_drm *drm,
  drmModeResPtr res,
  drmModeConnectorPtr conn
) {

  /* TODO would be really cool if we could make this work with a "disconnected" monitor, with X running.
   * Understandably, a monitor that X is using will just not be available, I've made peace with that.
   * If I disable the larger monitor via Mate monitors control panel, we still detect it here and try to connect.
   * Its encoder_id is zero, but (conn->encoders) does list the right one.
   * No matter what I do though, when we reach the first drmModeSetCrtc it fails.
   * I'm guessing we have to tell DRM to use this encoder with this connection, but no idea how...
   */

  drm->connid=conn->connector_id;
  drm->encid=conn->encoder_id;
  drm->screenw=drm->mode.hdisplay;
  drm->screenh=drm->mode.vdisplay;
  
  drmModeEncoderPtr encoder=drmModeGetEncoder(drm->fd,drm->encid);
  if (!encoder) return -1;
  drm->crtcid=encoder->crtc_id;
  drmModeFreeEncoder(encoder);
  
  // Store the current CRTC so we can restore it at quit.
  if (!(drm->crtc_restore=drmModeGetCrtc(drm->fd,drm->crtcid))) return -1;

  return 0;
}

/* Filter and compare modes.
 */
 
static int ma_drm_connector_acceptable(
  struct ma_drm *drm,
  drmModeConnectorPtr conn
) {
  if (conn->connection!=DRM_MODE_CONNECTED) return 0;
  // Could enforce size limits against (mmWidth,mmHeight), probably not.
  return 1;
}

static int ma_drm_mode_acceptable(
  struct ma_drm *drm,
  drmModeConnectorPtr conn,
  drmModeModeInfoPtr mode
) {
  //XXX temporary hack to prevent 30 Hz output from the Atari
  if (mode->vrefresh<50) return 0;
  //if (mode->hdisplay<3000) return 0;
  return 1;
}

// <0=a, >0=b, 0=whatever.
// All connections and modes provided here have been deemed acceptable.
// (conna) and (connb) will often be the same object.
static int ma_drm_mode_compare(
  struct ma_drm *drm,
  drmModeConnectorPtr conna,drmModeModeInfoPtr modea,
  drmModeConnectorPtr connb,drmModeModeInfoPtr modeb
) {
  
  // Firstly, if just one has the "PREFERRED" flag, it's in play right now. Prefer that, so we don't switch resolution.
  // Two modes with this flag, eg two monitors attached, they cancel out and we look at other things.
  //TODO I guess there should be an option to override this. As is, it should always trigger.
  int prefera=(modea->type&DRM_MODE_TYPE_PREFERRED);
  int preferb=(modeb->type&DRM_MODE_TYPE_PREFERRED);
  if (prefera&&!preferb) return -1;
  if (!prefera&&preferb) return 1;
  
  // Our framebuffer is surely smaller than any available mode, so prefer the smaller mode.
  if ((modea->hdisplay!=modeb->hdisplay)||(modea->vdisplay!=modeb->vdisplay)) {
    int areaa=modea->hdisplay*modea->vdisplay;
    int areab=modeb->hdisplay*modeb->vdisplay;
    if (areaa<areab) return -1;
    if (areaa>areab) return 1;
  }
  // Hmm ok, closest to 60 Hz?
  if (modea->vrefresh!=modeb->vrefresh) {
    int da=modea->vrefresh-60; if (da<0) da=-da;
    int db=modeb->vrefresh-60; if (db<0) db=-db;
    int dd=da-db;
    if (dd<0) return -1;
    if (dd>0) return 1;
  }

  // If we get this far, the modes are actually identical, so whatever.
  return 0;
}

/* Choose the best connector and mode.
 * The returned connector is STRONG.
 * Mode selection is copied over (G.mode).
 */
 
static drmModeConnectorPtr ma_drm_choose_connector(
  struct ma_drm *drm,
  drmModeResPtr res
) {

  //XXX? Maybe just while developing...
  int log_all_modes=0;
  int log_selected_mode=0;

  if (log_all_modes) {
    fprintf(stderr,"%s:%d: %s...\n",__FILE__,__LINE__,__func__);
  }
  
  drmModeConnectorPtr best=0; // if populated, (drm->mode) is also populated
  int conni=0;
  for (;conni<res->count_connectors;conni++) {
    drmModeConnectorPtr connq=drmModeGetConnector(drm->fd,res->connectors[conni]);
    if (!connq) continue;
    
    if (log_all_modes) fprintf(stderr,
      "  CONNECTOR %d %s %dx%dmm type=%08x typeid=%08x\n",
      connq->connector_id,
      (connq->connection==DRM_MODE_CONNECTED)?"connected":"disconnected",
      connq->mmWidth,connq->mmHeight,
      connq->connector_type,
      connq->connector_type_id
    );
    
    if (!ma_drm_connector_acceptable(drm,connq)) {
      drmModeFreeConnector(connq);
      continue;
    }
    
    drmModeModeInfoPtr modeq=connq->modes;
    int modei=connq->count_modes;
    for (;modei-->0;modeq++) {
    
      if (log_all_modes) fprintf(stderr,
        "    MODE %dx%d @ %d Hz, flags=%08x, type=%08x\n",
        modeq->hdisplay,modeq->vdisplay,
        modeq->vrefresh,
        modeq->flags,modeq->type
      );
    
      if (!ma_drm_mode_acceptable(drm,connq,modeq)) continue;
      
      if (!best) {
        best=connq;
        drm->mode=*modeq;
        continue;
      }
      
      if (ma_drm_mode_compare(drm,best,&drm->mode,connq,modeq)>0) {
        best=connq;
        drm->mode=*modeq;
        continue;
      }
    }
    
    if (best!=connq) {
      drmModeFreeConnector(connq);
    }
  }
  if (best&&log_selected_mode) {
    fprintf(stderr,
      "drm: selected mode %dx%d @ %d Hz on connector %d\n",
      drm->mode.hdisplay,drm->mode.vdisplay,
      drm->mode.vrefresh,
      best->connector_id
    );
  }
  return best;
}

/* File opened and resources acquired.
 */
 
static int ma_drm_init_with_res(
  struct ma_drm *drm,
  drmModeResPtr res
) {

  drmModeConnectorPtr conn=ma_drm_choose_connector(drm,res);
  if (!conn) {
    fprintf(stderr,"drm: Failed to locate a suitable connector.\n");
    return -1;
  }
  
  int err=ma_drm_init_with_conn(drm,res,conn);
  drmModeFreeConnector(conn);
  if (err<0) return -1;

  return 0;
}

/* Initialize connection.
 */
 
static int ma_drm_init_connection(struct ma_drm *drm) {

  const char *path="/dev/dri/card0";//TODO Let client supply this, or scan /dev/dri/
  
  if ((drm->fd=open(path,O_RDWR))<0) {
    fprintf(stderr,"%s: Failed to open DRM device: %m\n",path);
    return -1;
  }
  
  drmModeResPtr res=drmModeGetResources(drm->fd);
  if (!res) {
    fprintf(stderr,"%s:drmModeGetResources: %m\n",path);
    return -1;
  }
  
  int err=ma_drm_init_with_res(drm,res);
  drmModeFreeResources(res);
  if (err<0) return -1;
  
  return 0;
}

/* Initialize one framebuffer.
 */
 
static int ma_drm_fb_init(struct ma_drm *drm,struct ma_drm_fb *fb) {

  struct drm_mode_create_dumb creq={
    .width=drm->screenw,
    .height=drm->screenh,
    #if MA_PIXELSIZE==8
      .bpp=32,
    #else
      .bpp=16,
    #endif
    .flags=0,
  };
  if (ioctl(drm->fd,DRM_IOCTL_MODE_CREATE_DUMB,&creq)<0) {
    fprintf(stderr,"DRM_IOCTL_MODE_CREATE_DUMB: %m\n");
    return -1;
  }
  fb->handle=creq.handle;
  fb->size=creq.size;
  
  if (drmModeAddFB(
    drm->fd,
    creq.width,creq.height,
    #if MA_PIXELSIZE==8
      24,
    #else
      16,
    #endif
    creq.bpp,creq.pitch,
    fb->handle,
    &fb->fbid
  )<0) {
    fprintf(stderr,"drmModeAddFB: %m\n");
    return -1;
  }
  
  struct drm_mode_map_dumb mreq={
    .handle=fb->handle,
  };
  if (ioctl(drm->fd,DRM_IOCTL_MODE_MAP_DUMB,&mreq)<0) {
    fprintf(stderr,"DRM_IOCTL_MODE_MAP_DUMB: %m\n");
    return -1;
  }
  
  fb->v=mmap(0,fb->size,PROT_READ|PROT_WRITE,MAP_SHARED,drm->fd,mreq.offset);
  if (fb->v==MAP_FAILED) {
    fprintf(stderr,"mmap: %m\n");
    return -1;
  }
  
  return 0;
}

/* Given (fbw,fbh,screenw,screenh), calculate reasonable values for (scale,dstx,dsty,dstw,dsth).
 */
 
static int ma_drm_calculate_output_bounds(struct ma_drm *drm) {
  
  // Use the largest legal scale factor.
  // Too small is very unlikely -- our input is 96x64. So we're not going to handle that case, just fail.
  int scalex=drm->screenw/drm->fbw;
  int scaley=drm->screenh/drm->fbh;
  drm->scale=(scalex<scaley)?scalex:scaley;
  if (drm->scale<1) {
    fprintf(stderr,
      "Unable to fit %dx%d framebuffer on this %dx%d screen.\n",
      drm->fbw,drm->fbh,drm->screenw,drm->screenh
    );
    return -1;
  }
  if (drm->scale>MA_DRM_SCALE_LIMIT) drm->scale=MA_DRM_SCALE_LIMIT;
  
  drm->dstw=drm->scale*drm->fbw;
  drm->dsth=drm->scale*drm->fbh;
  drm->dstx=(drm->screenw>>1)-(drm->dstw>>1);
  drm->dsty=(drm->screenh>>1)-(drm->dsth>>1);
  
  return 0;
}

/* Init.
 */
 
static int ma_drm_init_buffers(struct ma_drm *drm) {
  
  if (ma_drm_fb_init(drm,drm->fbv+0)<0) return -1;
  if (ma_drm_fb_init(drm,drm->fbv+1)<0) return -1;
  
  if (ma_drm_calculate_output_bounds(drm)<0) return -1;
  
  if (drmModeSetCrtc(
    drm->fd,
    drm->crtcid,
    drm->fbv[0].fbid,
    0,0,
    &drm->connid,
    1,
    &drm->mode
  )<0) {
    fprintf(stderr,"drmModeSetCrtc: %m\n");
    return -1;
  }
  
  return 0;
}

/* Generate color table for 8-bit sources.
 */
 
#if MA_PIXELSIZE==8

static void ma_drm_generate_ctab(uint32_t *dst) {
  const int rshift=16;
  const int gshift=8;
  const int bshift=0;
  int src=0;
  for (;src<0x100;src++,dst++) {
    uint8_t rgb[3];
    ma_rgb_from_pixel(rgb,src);
    *dst=(rgb[0]<<rshift)|(rgb[1]<<gshift)|(rgb[2]<<bshift);
  }
}

#endif

/* New.
 */
 
struct ma_drm *ma_drm_new(int fbw,int fbh) {
  struct ma_drm *drm=calloc(1,sizeof(struct ma_drm));
  if (!drm) return 0;
  
  drm->fd=-1;
  drm->fbw=fbw;
  drm->fbh=fbh;
  
  #if MA_PIXELSIZE==8
    ma_drm_generate_ctab(drm->ctab);
  #endif
  
  if (
    (ma_drm_init_connection(drm)<0)||
    (ma_drm_init_buffers(drm)<0)
  ) {
    ma_drm_del(drm);
    return 0;
  }
  return drm;
}

/* Scale image into framebuffer.
 */
 
static void ma_drm_scale(struct ma_drm_fb *dst,struct ma_drm *drm,const ma_pixel_t *src) {
  ma_drm_pixel_t *dstrow=dst->v;
  dstrow+=drm->dsty*drm->screenw+drm->dstx;
  int cpc=drm->dstw*sizeof(ma_drm_pixel_t);
  const ma_pixel_t *srcrow=src;
  int yi=drm->fbh;
  for (;yi-->0;srcrow+=drm->fbw) {
    ma_drm_pixel_t *dstp=dstrow;
    const ma_pixel_t *srcp=srcrow;
    int xi=drm->fbw;
    for (;xi-->0;srcp++) {
    
      #if MA_PIXELSIZE==8
        ma_drm_pixel_t pixel=drm->ctab[*srcp];
      #else
        ma_drm_pixel_t pixel=*srcp;
      #endif
      
      int ri=drm->scale;
      for (;ri-->0;dstp++) *dstp=pixel;
    }
    dstp=dstrow;
    dstrow+=drm->screenw;
    int ri=drm->scale-1;
    for (;ri-->0;dstrow+=drm->screenw) {
      memcpy(dstrow,dstp,cpc);
    }
  }
}

/* Swap buffers.
 */
 
int ma_drm_swap(struct ma_drm *drm,const void *src) {
  
  drm->fbp^=1;
  struct ma_drm_fb *fb=drm->fbv+drm->fbp;
  
  ma_drm_scale(fb,drm,src);
  
  while (1) {
    if (drmModePageFlip(drm->fd,drm->crtcid,fb->fbid,0,drm)<0) {
      if (errno==EBUSY) { // waiting for prior flip
        usleep(1000);
        continue;
      }
      fprintf(stderr,"drmModePageFlip: %m\n");
      return -1;
    } else {
      break;
    }
  }
  
  return 0;
}
