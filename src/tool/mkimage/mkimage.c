#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/fs.h"
#include "common/png.h"

/* Convert image.
 */
 
static int text_from_png_rgba_image(void *dstpp,const struct png_image *image,const char *name,int namec,int pixelsize) {
  int dstc=0,dsta=4096;
  char *dst=malloc(dsta);
  if (!dst) return -1;
  
  // If there is at least one fully transparent pixel, set a colorkey.
  // We're not going to like analyze the image and select a little-used color for the key; it will be pure green only.
  int colorkey=0;
  const uint8_t *v=image->pixels;
  v+=3;
  int i=image->w*image->h;
  for (;i-->0;v++) if (!*v) {
    if (pixelsize==8) colorkey=0x1c;
    else colorkey=0xe007;
    break;
  }
  
  #define APPEND(fmt,...) { \
    while (1) { \
      int err=snprintf(dst+dstc,dsta-dstc,fmt,##__VA_ARGS__); \
      if (dstc<dsta-err) { \
        dstc+=err; \
        break; \
      } \
      if (dsta>100000) { \
        fprintf(stderr,"image too large\n"); \
        free(dst); \
        return -1; \
      } \
      void *nv=realloc(dst,dsta<<=1); \
      if (!nv) { \
        free(dst); \
        return -1; \
      } \
      dst=nv; \
    } \
  }
  
  APPEND("MA_IMAGE_DECLARE(%.*s,%d,%d,%d,\n  ",namec,name,image->w,image->h,colorkey)
  i=image->w*image->h;
  int dstw=0;
  const uint8_t *src=image->pixels;
  for (;i-->0;src+=4) {
    if (dstw>80) {
      APPEND("\n  ")
      dstw=0;
    }
    if (pixelsize==8) {
      uint8_t pixel;
      if (colorkey&&!src[3]) {
        pixel=colorkey;
      } else {
        pixel=(src[0]>>6)|((src[1]&0xe0)>>3)|(src[2]&0xe0);
        if (colorkey&&(pixel==colorkey)) pixel^=0x04; // flip the lowest green bit to avoid being the colorkey
      }
      APPEND("0x%02x,",pixel)
      dstw+=5;
    } else if (pixelsize==16) {
      uint16_t pixel;
      if (colorkey&&!src[3]) {
        pixel=colorkey;
      } else {
        pixel=((src[0]&0xf8)<<5)|((src[1]&0x1c)<<11)|(src[1]>>5)|(src[2]&0xf8);
        if (colorkey&&(pixel==colorkey)) pixel^=0x2000;
      }
      APPEND("0x%04x,",pixel)
      dstw+=7;
    }
  }
  APPEND("\n")
  APPEND(")\n")
  
  #undef APPEND
  *(void**)dstpp=dst;
  return dstc;
}
 
static int text_from_png_image(void *dstpp,const struct png_image *image,const char *name,int namec,int pixelsize) {
  struct png_image rgba={0};
  if (png_image_convert(&rgba,8,PNG_COLORTYPE_RGBA,image)<0) {
    png_image_cleanup(&rgba);
    return -1;
  }
    
  if (pixelsize) {
    int err=text_from_png_rgba_image(dstpp,&rgba,name,namec,pixelsize);
    png_image_cleanup(&rgba);
    return err;
  }
  char *a=0,*b=0;
  int ac=text_from_png_rgba_image(&a,&rgba,name,namec,8);
  int bc=text_from_png_rgba_image(&b,&rgba,name,namec,16);
  if ((ac<0)||(bc<0)) {
    if (a) free(a);
    png_image_cleanup(&rgba);
    return -1;
  }
  png_image_cleanup(&rgba);
  
  int dsta=ac+bc+64; // should need 56, add a few to be safe
  char *dst=malloc(dsta);
  if (!dst) { free(a); free(b); return -1; }
  int dstc=snprintf(dst,dsta,"#include \"multiarcade.h\"\n#if MA_PIXELSIZE==8\n%.*s\n#else\n%.*s\n#endif\n",ac,a,bc,b);
  free(a);
  free(b);
  if ((dstc<0)||(dstc>=dsta)) { free(dst); return -1; }
  *(void**)dstpp=dst;
  return dstc;
}

/* Main.
 */

int main(int argc,char **argv) {

  const char *dstpath=0,*srcpath=0,*name=0;
  int pixelsize=0;
  int argp=1;
  for (;argp<argc;argp++) {
    if (!memcmp(argv[argp],"-o",2)) {
      if (dstpath) {
        fprintf(stderr,"%s: Multiple output paths\n",argv[0]);
        return 1;
      }
      dstpath=argv[argp]+2;
    } else if (!memcmp(argv[argp],"-n",2)) {
      if (name) {
        fprintf(stderr,"%s: Multiple names\n",argv[0]);
        return 1;
      }
      name=argv[argp]+2;
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
    fprintf(stderr,"Usage: %s -oOUTPUT INPUT [-nNAME] [-s8|-s16]\n",argv[0]);
    fprintf(stderr,"  OUTPUT is a C file, INPUT is a PNG file.\n");
    return 1;
  }
  
  int namec=0;
  if (!name) {
    name=srcpath;
    int i=0; for (;srcpath[i];i++) if (srcpath[i]=='/') name=srcpath+i+1;
  }
  while (name[namec]&&(name[namec]!='.')) namec++;
  
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
  
  void *dst=0;
  int dstc=text_from_png_image(&dst,image,name,namec,pixelsize);
  if (dstc<0) {
    fprintf(stderr,"%s: Failed to convert image\n",srcpath);
    png_image_del(image);
    return 1;
  }
  png_image_del(image);
  
  int err=file_write(dstpath,dst,dstc);
  free(dst);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file\n",dstpath);
    return 1;
  }
  
  return 0;
}
