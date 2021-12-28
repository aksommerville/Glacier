#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/fs.h"
#include "common/png.h"

/* Convert image.
 */
 
extern const uint8_t ma_ctab8[768];

static inline uint8_t match_rgb(const uint8_t *src) {
  uint8_t best=0;
  int bestscore=9999;
  const uint8_t *ctab=ma_ctab8;
  int i=0; for (;i<256;i++,ctab+=3) {
    int dr=src[0]-ctab[0]; if (dr<0) dr=-dr;
    int dg=src[1]-ctab[1]; if (dg<0) dg=-dg;
    int db=src[2]-ctab[2]; if (db<0) db=-db;
    int score=dr+dg+db;
    if (!score) return i;
    if (score<bestscore) {
      best=i;
      bestscore=score;
    }
  }
  return best;
}
 
static void tsv_from_rgb(uint8_t *dst,const uint8_t *src,int c,int pixelsize) {
  if (pixelsize==8) {
    for (;c-->0;dst+=1,src+=3) {
      *dst=match_rgb(src);
    }
  } else {
    for (;c-->0;dst+=2,src+=3) {
      uint16_t bgr565=
        ((src[2]<<8)&0xf800)|
        ((src[1]<<3)&0x07e0)|
        (src[0]>>3)
      ;
      dst[0]=bgr565>>8;
      dst[1]=bgr565;
    }
  }
}
 
static int tsv_from_png_image(uint8_t *dst,const struct png_image *srcimage,int pixelsize) {
  struct png_image image={0};
  if (png_image_convert(&image,8,PNG_COLORTYPE_RGB,srcimage)<0) {
    png_image_cleanup(&image);
    return -1;
  }
  tsv_from_rgb(dst,image.pixels,96*64,pixelsize);
  png_image_cleanup(&image);
  return 0;
}

/* Main.
 */

int main(int argc,char **argv) {

  const char *dstpath=0,*srcpath=0;
  int pixelsize=16;
  int argp=1;
  for (;argp<argc;argp++) {
    if (!memcmp(argv[argp],"-o",2)) {
      if (dstpath) {
        fprintf(stderr,"%s: Multiple output paths\n",argv[0]);
        return 1;
      }
      dstpath=argv[argp]+2;
    } else if (!strcmp(argv[argp],"-s8")) {
      pixelsize=8;
    } else if (!strcmp(argv[argp],"-s16")) {
      pixelsize=16;
    } else if (!argv[argp][0]||(argv[argp][0]=='-')) {
      fprintf(stderr,"%s: Unexpected argument '%s'\n",argv[0],argv[argp]);
      return 1;
    } else if (srcpath) {
      fprintf(stderr,"%s: Multiple input paths\n",argv[0]);
      return 1;
    } else {
      srcpath=argv[argp];
    }
  }
  if (!dstpath||!srcpath) {
    fprintf(stderr,"Usage: %s -oOUTPUT INPUT [-s8]\n",argv[0]);
    fprintf(stderr,"  OUTPUT is a TinyArcade 'tsv' file, INPUT is a PNG file.\n");
    return 1;
  }
  
  void *src=0;
  int srcc=file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",srcpath);
    return 1;
  }
  
  struct png_image *image=png_decode(src,srcc);
  free(src);
  if (!image) {
    fprintf(stderr,"%s: Failed to decode %d-byte file as PNG\n",srcpath,srcc);
    return 1;
  }
  
  if ((image->w!=96)||(image->h!=64)) {
    fprintf(stderr,"%s: Dimensions must be 96x64. Found %dx%d\n",srcpath,image->w,image->h);
    png_image_del(image);
    return 1;
  }
  
  uint8_t *dst=malloc(96*64*(pixelsize>>3));
  if (!dst) {
    png_image_del(image);
    return 1;
  }
  
  if (tsv_from_png_image(dst,image,pixelsize)<0) {
    fprintf(stderr,"%s: Failed to convert image\n",srcpath);
    png_image_del(image);
    free(dst);
    return 1;
  }
  png_image_del(image);
  
  int err=file_write(dstpath,dst,96*64*(pixelsize>>3));
  free(dst);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file\n",dstpath);
    return 1;
  }
  
  return 0;
}
