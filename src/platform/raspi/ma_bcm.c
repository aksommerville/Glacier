#include "ma_raspi_internal.h"
#include <stdio.h>
#include <bcm_host.h>

#define MA_BCM_SIZE_LIMIT 4096 /* for no particular reason */
#define MA_BCM_SCALE_LIMIT 4 /* 2 is too blurry, 4 seems pretty nice */

struct ma_bcm {

  DISPMANX_DISPLAY_HANDLE_T vcdisplay;
  DISPMANX_RESOURCE_HANDLE_T vcresource;
  DISPMANX_ELEMENT_HANDLE_T vcelement;
  DISPMANX_UPDATE_HANDLE_T vcupdate;

  int fbw,fbh;
  int screenw,screenh;
  int scale;
  void *fb;
  int vsync_seq;
  #if MA_PIXELSIZE==8
    uint32_t ctab[256];
  #endif
  
  uint32_t blotter_storage;
};

/* Vsync.
 */

static void ma_bcm_cb_vsync(DISPMANX_UPDATE_HANDLE_T update,void *arg) {
  struct ma_bcm *bcm=arg;
  bcm->vsync_seq++;
}

/* Generate color table.
 */
#if MA_PIXELSIZE==8
static void ma_bcm_generate_ctab(uint32_t *dst) {
  uint8_t src=0;
  while (1) {
    
    uint8_t r=src&0x03; r<<=6; if (r&0x40) r|=0x20; r|=r>>3; r|=r>>6;
    uint8_t g=src&0x1f; g|=g<<3; g|=g>>6;
    uint8_t b=src&0xe0; b|=b>>3; b|=b>>6;

    *dst=(r<<16)|(g<<8)|b;

    if (src==0xff) return;
    src++;
    dst++;
  }
}
#endif

/* Calculate an appropriate intermediate framebuffer size, and output bounds.
 * A second framebuffer is necessary to convert the Tiny's pixels to RGB.
 * We also use it for software scaling, otherwise VideoCore interpolates and it's way too blurry.
 */

static int ma_bcm_measure_screen(VC_RECT_T *dstr,struct ma_bcm *bcm) {
  
  // First establish scaling factor. <BCM_SCALE_LIMIT and also within (screenw,screenh).
  int scalex=bcm->screenw/bcm->fbw;
  int scaley=bcm->screenh/bcm->fbh;
  int scale=(scalex<scaley)?scalex:scaley;
  if (scale<1) scale=1;
  else if (scale>MA_BCM_SCALE_LIMIT) scale=MA_BCM_SCALE_LIMIT;

  // Scale to the screen maintaining aspect ratio.
  int wforh=(bcm->fbw*bcm->screenh)/bcm->fbh;
  if (wforh<=bcm->screenw) {
    dstr->width=wforh;
    dstr->height=bcm->screenh;
  } else {
    dstr->width=bcm->screenw;
    dstr->height=(bcm->fbh*bcm->screenw)/bcm->fbw;
  }

  dstr->x=(bcm->screenw>>1)-(dstr->width>>1);
  dstr->y=(bcm->screenh>>1)-(dstr->height>>1);
  return scale;
}

/* Create a layer the size of the entire screen, since the Tiny's screen is
 * very unlikely to have the same aspect ratio as your TV.
 * Without this, edges of the console show through, it's ugly.
 */
 
static void ma_bcm_apply_blotter(struct ma_bcm *bcm) {
  bcm->blotter_storage=0;
  DISPMANX_RESOURCE_HANDLE_T res=vc_dispmanx_resource_create(
    VC_IMAGE_XRGB8888,1,1,&bcm->blotter_storage
  );
  if (!res) return;
  VC_RECT_T dstr={0,0,bcm->screenw,bcm->screenh};
  VC_RECT_T srcr={0,0,0x10000,0x10000};
  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xff};
  DISPMANX_ELEMENT_HANDLE_T element=vc_dispmanx_element_add(
    bcm->vcupdate,bcm->vcdisplay,1,&dstr,bcm->vcresource,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  );
  if (!element) return;
}

/* Initialize VideoCore, etc.
 */
 
static int ma_bcm_vc_init(struct ma_bcm *bcm) {

  graphics_get_display_size(0,&bcm->screenw,&bcm->screenh);
  if (
    (bcm->screenw<1)||(bcm->screenw>MA_BCM_SIZE_LIMIT)||
    (bcm->screenh<1)||(bcm->screenh>MA_BCM_SIZE_LIMIT)
  ) return -1;

  if (!(bcm->vcdisplay=vc_dispmanx_display_open(0))) return -1;
  if (!(bcm->vcupdate=vc_dispmanx_update_start(0))) return -1;

  VC_RECT_T dstr;
  bcm->scale=ma_bcm_measure_screen(&dstr,bcm);
  VC_RECT_T srcr={0,0,(bcm->fbw*bcm->scale)<<16,(bcm->fbh*bcm->scale)<<16};
  
  #if MA_PIXELSIZE==8
    int pixelsize=4;
    int fbformat=VC_IMAGE_XRGB8888;
  #else
    int pixelsize=2;
    int fbformat=VC_IMAGE_RGB565;
  #endif

  if (!(bcm->fb=malloc(pixelsize*bcm->fbw*bcm->scale*bcm->fbh*bcm->scale))) return -1;
  if (!(bcm->vcresource=vc_dispmanx_resource_create(
    fbformat,bcm->fbw*bcm->scale,bcm->fbh*bcm->scale,bcm->fb
  ))) return -1;

  VC_DISPMANX_ALPHA_T alpha={DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,0xff};
  if (!(bcm->vcelement=vc_dispmanx_element_add(
    bcm->vcupdate,bcm->vcdisplay,1,&dstr,bcm->vcresource,&srcr,DISPMANX_PROTECTION_NONE,&alpha,0,0
  ))) return -1;
  
  ma_bcm_apply_blotter(bcm);

  if (vc_dispmanx_update_submit_sync(bcm->vcupdate)<0) return -1;

  if (vc_dispmanx_vsync_callback(bcm->vcdisplay,ma_bcm_cb_vsync,bcm)<0) return -1;

  #if MA_PIXELSIZE==8
    ma_bcm_generate_ctab(bcm->ctab);
  #endif
  
  return 0;
}

/* New.
 */

struct ma_bcm *ma_bcm_new(int fbw,int fbh) {
  struct ma_bcm *bcm=calloc(1,sizeof(struct ma_bcm));
  if (!bcm) return 0;
  
  bcm->fbw=fbw;
  bcm->fbh=fbh;

  bcm_host_init();
  
  if (ma_bcm_vc_init(bcm)<0) {
    ma_bcm_del(bcm);
    return 0;
  }

  return bcm;
}

/* Delete.
 */

void ma_bcm_del(struct ma_bcm *bcm) {
  if (!bcm) return;
  if (bcm->fb) free(bcm->fb);
  bcm_host_deinit();
  free(bcm);
}

/* Swap buffers.
 */

static void ma_bcm_convert(void *output,struct ma_bcm *bcm,const void *input) {
  const ma_pixel_t *src=input;
  #if MA_PIXELSIZE==8
    uint32_t *dst=output;
  #else
    uint16_t *dst=output;
  #endif
  int dstw=bcm->fbw*bcm->scale;
  int cpc=bcm->fbw*bcm->scale*sizeof(*dst);
  int yi=bcm->fbh;
  for (;yi-->0;) {
    typeof(*dst) *dststart=dst;
    int xi=bcm->fbw;
    for (;xi-->0;src++) {
      #if MA_PIXELSIZE==8
        uint32_t pixel=bcm->ctab[*src];
      #else
        uint16_t pixel=((*src)>>8)|((*src)<<8); // first swap byte order
        pixel=(pixel&0x07e0)|(pixel>>11)|(pixel<<11); // then swap red and blue
      #endif
      int ri=bcm->scale;
      for (;ri-->0;dst++) *dst=pixel;
    }
    int ri=bcm->scale-1;
    for (;ri-->0;dst+=dstw) memcpy(dst,dststart,cpc);
  }
}

void ma_bcm_swap(struct ma_bcm *bcm,const void *fb) {
  ma_bcm_convert(bcm->fb,bcm,fb);
  VC_RECT_T fbr={0,0,bcm->fbw*bcm->scale,bcm->fbh*bcm->scale};
  #if MA_PIXELSIZE==8
    vc_dispmanx_resource_write_data(bcm->vcresource,VC_IMAGE_XRGB8888,bcm->fbw*bcm->scale*4,bcm->fb,&fbr);
  #else
    vc_dispmanx_resource_write_data(bcm->vcresource,VC_IMAGE_RGB565,bcm->fbw*bcm->scale*2,bcm->fb,&fbr);
  #endif
}
