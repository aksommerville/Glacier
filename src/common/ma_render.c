#include "multiarcade.h"

/* Fill rect.
 */
 
void ma_framebuffer_fill_rect(
  struct ma_framebuffer *dst,
  int16_t x,int16_t y,int16_t w,int16_t h,
  ma_pixel_t pixel
) {
  if (x<0) { w+=x; x=0; }
  if (x>dst->w-w) w=dst->w-x;
  if (w<1) return;
  if (y<0) { h+=y; y=0; }
  if (y>dst->h-h) h=dst->h-y;
  if (h<1) return;
  ma_pixel_t *row=dst->v+y*dst->stride+x;
  for (;h-->0;row+=dst->stride) {
    #if MA_PIXELSIZE==8
      memset(row,pixel,w);
    #else
      int16_t xi=w;
      ma_pixel_t *dstp=row;
      for (;xi-->0;dstp++) *dstp=pixel;
    #endif
  }
}

/* Blit.
 */
 
void ma_blit(
  struct ma_framebuffer *dst,
  int16_t dstx,int16_t dsty,
  const struct ma_texture *src,
  uint8_t xform
) {

  /* Clip against (dst).
   * Don't worry what that means for (src) space yet.
   */
  int16_t clipv[4]={0,0,0,0}; // l,r,u,d in dst
  int16_t w,h; // in dst, NB must be signed
  if (xform&MA_XFORM_SWAP) {
    w=src->h;
    h=src->w;
  } else {
    w=src->w;
    h=src->h;
  }
  if (dstx<0) { w+=dstx; clipv[0]=-dstx; dstx=0; }
  if (dsty<0) { h+=dsty; clipv[2]=-dsty; dsty=0; }
  if (dstx>dst->w-w) { clipv[1]=dstx+w-dst->w; w=dst->w-dstx; }
  if (dsty>dst->h-h) { clipv[3]=dsty+h-dst->h; h=dst->h-dsty; }
  if ((w<1)||(h<1)) return;
  
  /* Rearrange (clipv) according to (xform) so we can clip the input space.
   */
  int16_t srcclipv[4];
  if (xform&MA_XFORM_SWAP) {
    srcclipv[0]=clipv[2];
    srcclipv[1]=clipv[3];
    srcclipv[2]=clipv[0];
    srcclipv[3]=clipv[1];
  } else {
    srcclipv[0]=clipv[0];
    srcclipv[1]=clipv[1];
    srcclipv[2]=clipv[2];
    srcclipv[3]=clipv[3];
  }
  if (xform&MA_XFORM_XREV) {
    int16_t tmp=srcclipv[0]; srcclipv[0]=srcclipv[1]; srcclipv[1]=tmp;
  }
  if (xform&MA_XFORM_YREV) {
    int16_t tmp=srcclipv[2]; srcclipv[2]=srcclipv[3]; srcclipv[3]=tmp;
  }
  
  /* Prepare iterator against (src), always LRTB.
   */
  uint8_t srcstride=src->stride;
  const uint8_t *srcrow=src->v+srcstride*srcclipv[2]+(srcclipv[0]>>2);
  uint8_t srcshift0=6-((srcclipv[0]&3)<<1);
  
  /* Prepare a fancier iterator against (dst), generalizable to all transforms.
   */
  ma_pixel_t *dstp=dst->v+dst->stride*dsty+dstx;
  int16_t dminor,dmajor,dminorc,dmajorc;
  if (xform&MA_XFORM_SWAP) {
    dminor=dst->stride;
    dmajor=1;
    dminorc=h;
    dmajorc=w;
  } else {
    dminor=1;
    dmajor=dst->stride;
    dminorc=w;
    dmajorc=h;
  }
  if (xform&MA_XFORM_XREV) {
    dstp+=dminor*(dminorc-1);
    dminor=-dminor;
  }
  if (xform&MA_XFORM_YREV) {
    dstp+=dmajor*(dmajorc-1);
    dmajor=-dmajor;
  }
  dmajor-=dminor*dminorc;
  
  /* Iterate.
   */
  if (src->flags&MA_TEXTURE_TRANSPARENCY) {
    for (;dmajorc-->0;dstp+=dmajor,srcrow+=srcstride) {
      const uint8_t *srcp=srcrow;
      uint8_t srcshift=srcshift0;
      int16_t xi=dminorc;
      for (;xi-->0;dstp+=dminor) {
        uint8_t ix=((*srcp)>>srcshift)&3;
        if (ix) *dstp=src->ctab[ix];
        if (srcshift) srcshift-=2;
        else { srcshift=6; srcp++; }
      }
    }
  } else {
    for (;dmajorc-->0;dstp+=dmajor,srcrow+=srcstride) {
      const uint8_t *srcp=srcrow;
      uint8_t srcshift=srcshift0;
      int16_t xi=dminorc;
      for (;xi-->0;dstp+=dminor) {
        uint8_t ix=((*srcp)>>srcshift)&3;
        *dstp=src->ctab[ix];
        if (srcshift) srcshift-=2;
        else { srcshift=6; srcp++; }
      }
    }
  }
  
}

/* Incremental transform.
 */
 
static uint8_t ma_adjust_90[8]={
  MA_XFORM_SWAP|MA_XFORM_YREV,
  MA_XFORM_SWAP|MA_XFORM_XREV|MA_XFORM_YREV,
  MA_XFORM_SWAP,
  MA_XFORM_SWAP|MA_XFORM_XREV,
  MA_XFORM_XREV,
  0,
  MA_XFORM_XREV|MA_XFORM_YREV,
  MA_XFORM_YREV,
};

static uint8_t ma_adjust_270[8]={
  MA_XFORM_SWAP|MA_XFORM_XREV,
  MA_XFORM_SWAP,
  MA_XFORM_SWAP|MA_XFORM_XREV|MA_XFORM_YREV,
  MA_XFORM_SWAP|MA_XFORM_YREV,
  MA_XFORM_YREV,
  MA_XFORM_XREV|MA_XFORM_YREV,
  0,
  MA_XFORM_XREV,
};
 
uint8_t ma_xform_adjust(uint8_t prev,uint8_t adjustment) {
  switch (adjustment) {
    case MA_XFORM_ADJUST_X: return prev^((prev&MA_XFORM_SWAP)?MA_XFORM_YREV:MA_XFORM_XREV);
    case MA_XFORM_ADJUST_Y: return prev^((prev&MA_XFORM_SWAP)?MA_XFORM_XREV:MA_XFORM_YREV);
    case MA_XFORM_ADJUST_90: return ma_adjust_90[prev&7];
    case MA_XFORM_ADJUST_180: return prev^(MA_XFORM_XREV|MA_XFORM_YREV);
    case MA_XFORM_ADJUST_270: return ma_adjust_270[prev&7];
  }
  return prev;
}

/* Structured grid.
 */
 
void ma_render_grid(
  struct ma_framebuffer *dst,
  const uint8_t *src,
  uint8_t colc,uint8_t rowc,
  uint16_t stride,
  const struct ma_tile *tilemap,
  const void *pixels,
  uint8_t tilesize,
  uint16_t bg
) {
  int16_t dsty=0;
  for (;rowc-->0;src+=stride,dsty+=tilesize) {
    uint8_t xi=colc;
    const uint8_t *srcp=src;
    int16_t dstx=0;
    for (;xi-->0;srcp++,dstx+=tilesize) {
      const struct ma_tile *tile=tilemap+(*srcp);
      
      if (tile->xform&MA_TEXTURE_TRANSPARENCY) {
        switch (bg&0xff00) {
          case MA_GRID_BG_CELL: {
              uint8_t bgcell=bg;
              if (bgcell!=(*srcp)) {
                ma_render_sprite(dst,dstx,dsty,tilemap+bgcell,pixels,tilesize);
              }
            } break;
          case MA_GRID_BG_COLOR: {
              ma_framebuffer_fill_rect(dst,dstx,dsty,tilesize,tilesize,bg);
            } break;
        }
      }
      
      ma_render_sprite(dst,dstx,dsty,tile,pixels,tilesize);
    }
  }
}

/* Structured sprite.
 */

void ma_render_sprite(
  struct ma_framebuffer *dst,
  int16_t dstx,int16_t dsty,
  const struct ma_tile *tile,
  const void *pixels,
  uint8_t tilesize
) {
  uint16_t srcx=(tile->tileid&0x0f)*tilesize;
  uint16_t srcy=(tile->tileid>>4)*tilesize;
  struct ma_texture tex={
    .w=tilesize,
    .h=tilesize,
    .stride=tilesize<<2, // *16/4
    .flags=MA_TEXTURE_TRANSPARENCY&tile->xform,
    .ctab={tile->ctab[0],tile->ctab[1],tile->ctab[2],tile->ctab[3]},
    .v=(uint8_t*)pixels+(tilesize<<2)*srcy+(srcx>>2),
  };
  ma_blit(dst,dstx,dsty,&tex,tile->xform&(MA_XFORM_XREV|MA_XFORM_YREV|MA_XFORM_SWAP));
}
