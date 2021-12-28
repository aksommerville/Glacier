#include "mktex_tilesheet.h"
#include "../common/multiarcade.h"
#include "common/png.h"
#include "common/encoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

/* Cleanup.
 */
 
void mktex_tilesheet_cleanup(struct mktex_tilesheet *tilesheet) {
  if (tilesheet->tilev) {
    struct mktex_tile *tile=tilesheet->tilev;
    int i=tilesheet->tilec;
    for (;i-->0;tile++) {
      if (tile->v) free(tile->v);
    }
    free(tilesheet->tilev);
  }
  if (tilesheet->name) free(tilesheet->name);
  memset(tilesheet,0,sizeof(struct mktex_tilesheet));
}

/* Search tiles.
 */
 
static int mktex_tilesheet_search(const struct mktex_tilesheet *tilesheet,uint8_t tileid) {
  if (tileid<tilesheet->tilecontigc) return tileid;
  int lo=0,hi=tilesheet->tilec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
         if (tileid<tilesheet->tilev[ck].tileid) hi=ck;
    else if (tileid>tilesheet->tilev[ck].tileid) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

/* Add tile.
 */
 
static struct mktex_tile *mktex_tilesheet_add_tile(struct mktex_tilesheet *tilesheet,uint8_t tileid) {

  int p=mktex_tilesheet_search(tilesheet,tileid);
  if (p>=0) return 0;
  p=-p-1;
  
  if (tilesheet->tilea>=tilesheet->tilec) {
    int na=tilesheet->tilea+256; // no sense conserving, go all the way
    void *nv=realloc(tilesheet->tilev,sizeof(struct mktex_tile)*na);
    if (!nv) return 0;
    tilesheet->tilev=nv;
    tilesheet->tilea=na;
  }
  
  struct mktex_tile *tile=tilesheet->tilev+p;
  memmove(tile+1,tile,sizeof(struct mktex_tile)*(tilesheet->tilec-p));
  memset(tile,0,sizeof(struct mktex_tile));
  tile->tileid=tileid;
  tilesheet->tilec++;

  while (
    (tilesheet->tilecontigc<tilesheet->tilec)&&
    (tilesheet->tilev[tilesheet->tilecontigc].tileid==tilesheet->tilecontigc)
  ) tilesheet->tilecontigc++;
  
  return tile;
}

/* Image analysis.
 * For single images in isolation. Cross-image analysis is a separate process.
 */
 
struct mktex_analysis {
  int has_transparency; // At least one pixel has alpha zero.
  int fully_transparent; // Every alpha is zero.
  int intermediate_alpha; // At least one pixel has alpha not 0 or 255.
  int colorc; // Count of unique colors.
  uint32_t colorv[4]; // xrgb. If (colorc>4), we safely only store the first 4.
  int colorc_overrun; // Nonzero if there's way too many colors, and (colorc) probably overcounted.
};

static void mktex_analyze(
  struct mktex_analysis *analysis,
  const uint8_t *src,int stride,
  int w,int h
) {

  analysis->has_transparency=0;
  analysis->fully_transparent=1;
  analysis->intermediate_alpha=0;
  analysis->colorc=0;
  analysis->colorc_overrun=0;
  
  // We keep a separate list of colors, in order to keep the count accurate in small overuse cases.
  // Eventually it could fill up and we'd count the same color as multiple duplicates. Not a big deal.
  #define allcolora 64
  uint32_t allcolorv[allcolora];

  for (;h-->0;src+=stride) {
    const uint8_t *p=src;
    int xi=w;
    for (;xi-->0;p+=4) {
      if (!p[3]) {
        analysis->has_transparency=1;
        continue;
      }
      analysis->fully_transparent=0;
      if (p[3]!=0xff) {
        analysis->intermediate_alpha=1;
      }
      uint32_t color=(p[0]<<16)|(p[1]<<8)|p[2];
      int i=analysis->colorc,already=0;
      if (i>allcolora) i=allcolora;
      while (i-->0) {
        if (allcolorv[i]==color) {
          already=1;
          break;
        }
      }
      if (!already) {
        if (analysis->colorc<allcolora) {
          allcolorv[analysis->colorc]=color;
        } else {
          analysis->colorc_overrun=1;
        }
        analysis->colorc++;
      }
    }
  }
  
  memcpy(analysis->colorv,allcolorv,sizeof(analysis->colorv));
  
  #undef allcolora
}

/* Convert four XRGB colors to TinyArcade's 8 and 16 bit pixels.
 * Also split it into natural RGB, will be helpful at convert.
 */
 
static void mktex_generate_ctab(
  uint8_t *dstrgb,
  uint8_t *dst8,
  uint16_t *dst16,
  const uint32_t *src
) {
  int i=4; for (;i-->0;dstrgb+=3,dst8++,dst16++,src++) {
    uint8_t rgb[3]={(*src)>>16,(*src)>>8,*src};
    *dst16=((rgb[0]&0xf8)<<5)|((rgb[1]&0x1c)<<11)|(rgb[1]>>5)|(rgb[2]&0xf8);
    memcpy(dstrgb,rgb,3);
    
    #if 0 // XXX Naive fast 8-bit BGR332. This mismatches the Tiny's output in a few places.
      *dst8=(rgb[0]>>6)|((rgb[1]&0xe0)>>3)|(rgb[2]&0xe0);
    #else // Expensive reverse-lookup in the color table.
      *dst8=0;
      int score=769;
      const uint8_t *src=ma_ctab8;
      int ix=0; for (;ix<256;ix++,src+=3) {
        int dr=src[0]-rgb[0]; if (dr<0) dr=-dr;
        int dg=src[1]-rgb[1]; if (dg<0) dg=-dg;
        int db=src[2]-rgb[2]; if (db<0) db=-db;
        int d=dr+dg+db;
        if (d<score) {
          *dst8=ix;
          if (!d) break; // not going to get closer than this
          score=d;
        }
      }
    #endif
  }
}

/* Convert and copy pixels into prepared tile.
 * Buffer, dimensions, flags, and color tables must be final first.
 */
 
static void mktex_tile_receive_input(
  struct mktex_tile *tile,
  const uint8_t *src,
  int stride,
  const uint8_t *ctab, /* rgb*4, 12 bytes */
  int validw /* may be slightly smaller than (tile->w) due to padding; don't read beyond this */
) {
  uint8_t *dst=tile->v;
  uint8_t gather=0;
  int shift=6;
  int yi=tile->h;
  for (;yi-->0;src+=stride) {
    const uint8_t *srcp=src;
    int xi=validw;
    for (;xi-->0;srcp+=4) {
      
      // Transparent pixels are zero. Only need to look up the other ones.
      // This might be a little overkill, finding the closest match: Non-exact-match is kind of an error on the user's part. Whatever.
      int ix=0;
      if (srcp[3]) {
        int cc=4,ci=0;
        const uint8_t *cq=ctab;
        if (tile->flags&MA_TEXTURE_TRANSPARENCY) {
          cc=3;
          cq+=3;
        }
        int bestd=1024;
        for (;ci<cc;ci++,cq+=3) {
          int dr=cq[0]-srcp[0]; if (dr<0) dr=-dr;
          int dg=cq[1]-srcp[1]; if (dg<0) dg=-dg;
          int db=cq[2]-srcp[2]; if (db<0) db=-db;
          int d=dr+dg+db;
          if (!d) {
            ix=ci;
            break;
          }
          if (d<bestd) {
            ix=ci;
            bestd=d;
          }
        }
        // If transparency in play, we were looking at indices 1..3. Add one.
        if (tile->flags&MA_TEXTURE_TRANSPARENCY) ix++;
      }
      
      gather|=ix<<shift;
      if (shift) {
        shift-=2;
      } else {
        shift=6;
        *dst=gather;
        dst++;
        gather=0;
      }
    }
    if (shift!=6) {
      shift=6;
      *dst=gather;
      dst++;
      gather=0;
    }
  }
}

/* Receive one tile image.
 */
 
static int mktex_convert_sub_image(
  struct mktex_tilesheet *tilesheet,
  const void *pixels,
  int stride,int w,int h,
  const char *name,int namec,
  int tileid // <0 if not a tile
) {

  /* Analyze, issue warnings, short-circuit if possible.
   * There is no analysis case for which we fail, but there's a couple warnings.
   */
  struct mktex_analysis analysis={0};
  mktex_analyze(&analysis,pixels,stride,w,h);
  if (analysis.fully_transparent&&!tilesheet->singleton) return 0; // ignore transparent tiles
  if (analysis.intermediate_alpha) {
    fprintf(stderr,"%.*s:%02x: At least one pixel with alpha in 1..254 will be treated like 255\n",namec,name,tileid);
  }
  if (analysis.colorc_overrun||((analysis.colorc==4)&&analysis.has_transparency)) {
    fprintf(stderr,
      "%.*s:%02x: Detected %d unique colors. Limit %d.\n",
      namec,name,tileid,analysis.colorc,
      analysis.has_transparency?3:4
    );
  }
  
  /* Create a tile.
   */
  struct mktex_tile *tile=mktex_tilesheet_add_tile(tilesheet,tileid);
  if (!tile) return -1;
  tile->w=(w+3)&~3; // round up to 4
  tile->h=h;
  
  /* Generate color table.
   * Prep for transparency if applicable.
   */
  uint8_t scratchctab[12];
  mktex_generate_ctab(scratchctab,tile->ctab8,tile->ctab16,analysis.colorv);
  tile->colorc=analysis.colorc;
  if (analysis.has_transparency) {
    memmove(scratchctab+3,scratchctab,9);
    memmove(tile->ctab8+1,tile->ctab8,sizeof(uint8_t)*3);
    memmove(tile->ctab16+1,tile->ctab16,sizeof(uint16_t)*3);
    scratchctab[0]=scratchctab[1]=scratchctab[2]=0;
    tile->ctab8[0]=0;
    tile->ctab16[0]=0;
    tile->colorc++;
    tile->flags|=MA_TEXTURE_TRANSPARENCY;
  }
  if (tile->colorc>4) tile->colorc=4;
  
  /* Allocate tile pixels.
   * Convert image.
   */
  int vsize=(tile->w*tile->h)>>2;
  if (!(tile->v=malloc(vsize))) return -1;
  mktex_tile_receive_input(tile,pixels,stride,scratchctab,w);
  
  return 0;
}

/* Given a non-transparent pixel in some image, return the sub-image's boundary.
 * A 1-pixel transparent border is required but not included. We fail if it's missing.
 */

static int mktex_rect_is_empty(const struct png_image *image,int x,int y,int w,int h) {
  const uint8_t *p=(uint8_t*)image->pixels+y*image->stride+x*4+3;
  for (;h-->0;p+=image->stride) {
    const uint8_t *xp=p;
    int xi=w;
    for (;xi-->0;xp+=4) {
      if (*xp) return 0;
    }
  }
  return 1;
}

static int mktex_auto_measure(int *srcx,int *srcy,int *srcw,int *srch,const struct png_image *image,int x,int y) {

  // Initially a 1x1 box.
  *srcx=x;
  *srcy=y;
  *srcw=1;
  *srch=1;

  /* Try to extend each of the four edges until they all have fully transparent neighbors.
   * If we reach the image's edge, it's an error.
   * This approach lets us avoid using a seed-fill buffer.
   */
  while (1) {

    // Edge?
    if (*srcx<=0) return -1;
    if (*srcy<=0) return -1;
    if ((*srcx)+(*srcw)>=image->w) return -1;
    if ((*srcy)+(*srch)>=image->h) return -1;

    // Extend?
    int done=1;
    if (!mktex_rect_is_empty(image,(*srcx)-1,*srcy,1,*srch)) { (*srcx)--; (*srcw)++; done=0; }
    if (!mktex_rect_is_empty(image,*srcx,(*srcy)-1,*srcw,1)) { (*srcy)--; (*srch)++; done=0; }
    if (!mktex_rect_is_empty(image,(*srcx)+(*srcw),*srcy,1,*srch)) { (*srcw)++; done=0; }
    if (!mktex_rect_is_empty(image,*srcx,(*srcy)+(*srch),*srcw,1)) { (*srch)++; done=0; }
    if (done) return 0;
  }
}

/* Clear rectangle in the source image.
 * Necessary for auto-measure, so we don't detect the sub-image twice.
 */

static void mktex_clear_rect(struct png_image *image,int x,int y,int w,int h) {
  uint8_t *p=(uint8_t*)image->pixels+y*image->stride+x*4;
  int size=w*4;
  for (;h-->0;p+=image->stride) memset(p,0,size);
}

/* Read image: Split out cells and defer.
 */
 
int mktex_tilesheet_read_image(
  struct mktex_tilesheet *tilesheet,
  struct png_image *image,
  const char *name,int namec,
  int tilesize
) {

  if (tilesheet->name) return -1;
  if (!(tilesheet->name=malloc(namec+1))) return -1;
  memcpy(tilesheet->name,name,namec);
  tilesheet->name[namec]=0;
  tilesheet->namec=namec;

  // Loose decals.
  if (tilesize<1) {
    int tileid=0;
    uint8_t *p=image->pixels;
    int y=0; for (;y<image->h;y++) {
      int x=0; for (;x<image->w;x++,p+=4) {
        if (!p[3]) continue;
        int srcx,srcy,srcw,srch;
        if (mktex_auto_measure(&srcx,&srcy,&srcw,&srch,image,x,y)<0) {
          fprintf(stderr,"%.*s: Measurement fault around pixel (%d,%d)\n",namec,name,x,y);
          return -1;
        }
        if (tileid>=0x100) {
          fprintf(stderr,"%.*s: Too many sub-images, limit 256\n",namec,name);
          return -1;
        }
        if (mktex_convert_sub_image(
          tilesheet,
          (uint8_t*)image->pixels+image->stride*srcy+srcx*4,
          image->stride,
          srcw,srch,
          name,namec,
          tileid
        )<0) return -1;
        //fprintf(stderr,"%.*s: Tile 0x%02x @ %d,%d,%d,%d\n",namec,name,tileid,srcx,srcy,srcw,srch);
        tileid++;
        mktex_clear_rect(image,srcx,srcy,srcw,srch);
      }
    }

  // Uniform square tiles.
  } else if (tilesize) {
    if ((tilesize<1)||(tilesize>255)||(tilesize&3)) {
      fprintf(stderr,"%.*s: Invalid tilesize %d\n",namec,name,tilesize);
      return -1;
    }
    int colc=image->w/tilesize;
    int rowc=image->h/tilesize;
    if ((colc<1)||(rowc<1)||(colc>16)||(rowc>16)) {
      fprintf(stderr,
        "%.*s: Invalid cell geometry %dx%d, must be 1..16 columns and rows\n",
        namec,name,colc,rowc
      );
      return -1;
    }
    int row=0;
    for (;row<rowc;row++) {
      int col=0;
      for (;col<colc;col++) {
        if (mktex_convert_sub_image(
          tilesheet,
          (uint8_t*)image->pixels+image->stride*row*tilesize+col*tilesize*4,
          image->stride,
          tilesize,tilesize,
          name,namec,
          (row<<4)|col
        )<0) return -1;
      }
    }
    
  // Single image, not tiles.
  } else {
    tilesheet->singleton=1;
    if ((image->w<1)||(image->h<1)||(image->w>255)||(image->h>255)||(image->w&3)) {
      fprintf(stderr,
        "%.*s: Invalid dimensions %dx%d. Must be 1..255, and width must be multiple of 4\n",
        namec,name,image->w,image->h
      );
      return -1;
    }
    if (mktex_convert_sub_image(
      tilesheet,
      image->pixels,
      image->stride,
      image->w,image->h,
      name,namec,
      -1
    )<0) return -1;
  }
  return 0;
}

/* Optimize tilesheet.
 */
 
int mktex_tilesheet_optimize(
  struct mktex_tilesheet *tilesheet
) {
  struct mktex_tile *a=tilesheet->tilev+1;
  int ai=1;
  for (;ai<tilesheet->tilec;ai++,a++) {
    
    // If somehow (a) is already borrowing, skip it.
    if (!a->v) continue;
    
    int size=(a->w*a->h)>>2;
    
    const struct mktex_tile *b=tilesheet->tilev;
    int bi=0;
    for (;bi<ai;bi++,b++) {
    
      // We can only borrow pixels from an image that has them.
      // (pretty sure this is not possible; we'd have already matched the one it's borrowing from).
      if (!b->v) continue;
      
      // Transparent can only match transparent, and opaque opaque.
      if ((a->flags&MA_TEXTURE_TRANSPARENCY)!=(b->flags&MA_TEXTURE_TRANSPARENCY)) continue;
      
      // Proceed only if dimensions match.
      // We don't support tilesheets with various dimensions, but let's be safe.
      //TODO In theory, we should check ((aw==bh)&&(ah==bw)) for SWAP xforms. But in practice, tiles will always be square.
      if ((a->w!=b->w)||(a->h!=b->h)) continue;
      
      // Easiest case: Pixels match exactly and vary only by ctab.
      if (!memcmp(a->v,b->v,size)) {
        free(a->v);
        a->v=0;
        a->vborrow=b->tileid;
        if (!memcmp(a->ctab16,b->ctab16,sizeof(a->ctab16))) {
          fprintf(stderr,
            "%.*s:WARNING: Tiles 0x%02x and 0x%02x are exactly the same.\n",
            tilesheet->namec,tilesheet->name,
            b->tileid,a->tileid
          );
        } else {
          fprintf(stderr,"tile 0x%02x borrowing from 0x%02x\n",a->tileid,b->tileid);
        }
        break;
      }
      
      //TODO Compare tiles with xform
    }
  }
  return 0;
}

/* Generate output.
 */

int mktex_tilesheet_encode(
  struct encoder *encoder,
  struct mktex_tilesheet *tilesheet
) {
  if (encoder_append(encoder,"#include \"multiarcade.h\"\n\n",-1)<0) return -1;
  
  // Start with the raw pixels for everything that has them.
  const struct mktex_tile *tile=tilesheet->tilev;
  int i=tilesheet->tilec;
  for (;i-->0;tile++) {
    if (!tile->v) continue;
    char name[64];
    int namec;
    if (tilesheet->singleton) namec=snprintf(name,sizeof(name),"%.*s",tilesheet->namec,tilesheet->name);
    else namec=snprintf(name,sizeof(name),"%.*s_%02x",tilesheet->namec,tilesheet->name,tile->tileid);
    if ((namec<1)||(namec>=sizeof(name))) return -1;

    if (encoder_appendf(encoder,"uint8_t tex_%.*s_storage[]={",namec,name)<0) return -1;
    int bytec=(tile->w*tile->h)>>2;
    const uint8_t *v=tile->v;
    for (;bytec-->0;v++) if (encoder_appendf(encoder,"%d,",*v)<0) return -1;
    if (encoder_append(encoder,"};\n",3)<0) return -1;
  }
  
  if (encoder_append(encoder,"\n",1)<0) return -1;
  
  // Next, produce texture objects for everything.
  for (tile=tilesheet->tilev,i=tilesheet->tilec;i-->0;tile++) {
    char name[64],pxname[64];
    int namec,pxnamec;
    if (tilesheet->singleton) namec=snprintf(name,sizeof(name),"%.*s",tilesheet->namec,tilesheet->name);
    else namec=snprintf(name,sizeof(name),"%.*s_%02x",tilesheet->namec,tilesheet->name,tile->tileid);
    if ((namec<1)||(namec>=sizeof(name))) return -1;
    if (tile->v) {
      memcpy(pxname,name,namec);
      pxnamec=namec;
    } else {
      pxnamec=snprintf(pxname,sizeof(pxname),"%.*s_%02x",tilesheet->namec,tilesheet->name,tile->vborrow);
      if ((pxnamec<1)||(pxnamec>=sizeof(pxname))) return -1;
    }
 
    if (encoder_appendf(encoder,"struct ma_texture tex_%.*s={\n",namec,name)<0) return -1;
    if (encoder_appendf(encoder,".w=%d,.h=%d,\n",tile->w,tile->h)<0) return -1;
    if (encoder_appendf(encoder,".stride=%d,\n",tile->w>>2)<0) return -1;
    if (encoder_appendf(encoder,".flags=%d,\n",tile->flags)<0) return -1;
    if (encoder_appendf(encoder,"#if MA_PIXELSIZE==8\n")<0) return -1;
    if (encoder_appendf(encoder,".ctab={%d,%d,%d,%d},\n",tile->ctab8[0],tile->ctab8[1],tile->ctab8[2],tile->ctab8[3])<0) return -1;
    if (encoder_appendf(encoder,"#else\n")<0) return -1;
    if (encoder_appendf(encoder,".ctab={%d,%d,%d,%d},\n",tile->ctab16[0],tile->ctab16[1],tile->ctab16[2],tile->ctab16[3])<0) return -1;
    if (encoder_appendf(encoder,"#endif\n")<0) return -1;
    if (encoder_appendf(encoder,".v=tex_%.*s_storage,\n",pxnamec,pxname)<0) return -1;
    if (encoder_appendf(encoder,"};\n\n")<0) return -1;
  }
  
  if (0) { //XXX For curiosity's sake, how much memory does it all take?
    if (encoder_appendf(encoder,"uint32_t tex_memory_usage_%.*s=0",tilesheet->namec,tilesheet->name)<0) return -1;
    for (tile=tilesheet->tilev,i=tilesheet->tilec;i-->0;tile++) {
      char name[64];
      int namec;
      if (tilesheet->singleton) namec=snprintf(name,sizeof(name),"%.*s",tilesheet->namec,tilesheet->name);
      else namec=snprintf(name,sizeof(name),"%.*s_%02x",tilesheet->namec,tilesheet->name,tile->tileid);
      if ((namec<1)||(namec>=sizeof(name))) return -1;
      if (tile->v) {
        if (encoder_appendf(encoder,"+sizeof(tex_%.*s_storage)",namec,name)<0) return -1;
      }
      if (encoder_appendf(encoder,"+sizeof(tex_%.*s)",namec,name)<0) return -1;
    }
    if (encoder_append(encoder,";\n",2)<0) return -1;
  }
  
  return 0;
}
