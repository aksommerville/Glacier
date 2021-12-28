#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/fs.h"
#include "common/png.h"

/* Convert image.
 */
 
static int font_text_from_glyphs(void *dstpp,const uint8_t *metrics,const uint32_t *bits,const char *name,int namec) {
  int dstc=0,dsta=4096;
  char *dst=malloc(dsta);
  if (!dst) return -1;
  
  #define APPEND(fmt,...) { \
    while (1) { \
      int err=snprintf(dst+dstc,dsta-dstc,fmt,##__VA_ARGS__); \
      if (dstc<dsta-err) { \
        dstc+=err; \
        break; \
      } \
      if (dsta>32768) { \
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
  
  int y,x;
  APPEND("#include \"multiarcade.h\"\n")
  APPEND("const struct ma_font font_%.*s={\n",namec,name)
  APPEND("  .metrics={\n")
  for (y=6;y-->0;) {
    APPEND("    ")
    for (x=16;x-->0;metrics++) {
      APPEND("0x%02x,",*metrics)
    }
    APPEND("\n")
  }
  APPEND("  },\n  .bits={\n")
  for (y=12;y-->0;) {
    APPEND("    ")
    for (x=8;x-->0;bits++) {
      APPEND("0x%08x,",*bits)
    }
    APPEND("\n")
  }
  APPEND("  },\n};\n")
  
  #undef APPEND
  *(void**)dstpp=dst;
  return dstc;
}
 
static int font_glyph_from_y1(uint8_t *metrics,uint32_t *bits,const uint8_t *src,int stride) {

  int h=8,y=0;
  while (h&&!*src&&(y<3)) { h--; src+=stride; y++; }
  while (h&&!src[(h-1)*stride]) h--;
  if (!h) return 0;
  
  uint8_t aggregate=0;
  int i=h;
  while (i-->0) aggregate|=src[i*stride];
  int w=8,x=0;
  while (w&&!(aggregate&(1<<(8-w)))) w--;
  while (w&&!(aggregate&0x80)) { x++; w--; aggregate<<=1; }
  
  if ((w>7)||(h>7)||(y>3)) {
    fprintf(stderr,"Invalid (w,h,y) = (%d,%d,%d), limit (7,7,3)\n",w,h,y);
    return -1;
  }
  *metrics=(w<<5)|(h<<2)|y;
  
  int shift=0;
  for (i=h;i-->0;src+=stride,shift+=w) {
    (*bits)|=((uint32_t)((*src)<<(x+24)))>>shift;
  }
  
  return 0;
}
 
static int font_text_from_png_y1_image(void *dstpp,const struct png_image *image,const char *name,int namec) {
  
  const int colw=8;
  const int rowh=8;
  const int colc=16;
  const int rowc=6;
  
  if ((image->w!=colw*colc)||(image->h!=rowh*rowc)) {
    fprintf(stderr,
      "Dimensions must be exactly %dx%d (%dx%d cells of %dx%d pixels). Found %dx%d\n",
      colc*colw,rowc*rowh,colc,rowc,colw,rowh,image->w,image->h
    );
    return -1;
  }
  
  // Cells being 8 pixels wide is a nice convenience.
  // We've converted to 1-bit luma, so each row is a byte nice and neat.
  
  uint8_t metrics[0x60]={0};
  uint32_t bits[0x60]={0};
  int col=0,row=0;
  const uint8_t *pxp=image->pixels;
  int p=0; for (;p<0x60;p++,pxp++) {
    if (font_glyph_from_y1(metrics+p,bits+p,pxp,image->stride)<0) {
      fprintf(stderr,"Error digesting glyph 0x%02x\n",p+0x20);
      return -1;
    }
    
    // 0x20 is special: It's expected to be empty, but we'll cheat and say it's 3 pixels wide.
    if (!p&&!metrics[p]&&!bits[p]) {
      metrics[p]=0x60;
    }
    
    if (++col>=colc) {
      col=0;
      row++;
      pxp+=image->stride*(rowh-1);
    }
  }
  
  return font_text_from_glyphs(dstpp,metrics,bits,name,namec);
}
 
static int font_text_from_png_image(void *dstpp,const struct png_image *image,const char *name,int namec) {
  struct png_image y1={0};
  if (png_image_convert(&y1,1,PNG_COLORTYPE_GRAY,image)<0) {
    png_image_cleanup(&y1);
    return -1;
  }
  int err=font_text_from_png_y1_image(dstpp,&y1,name,namec);
  png_image_cleanup(&y1);
  return err;
}

/* Main.
 */

int main(int argc,char **argv) {

  const char *dstpath=0,*srcpath=0,*name=0;
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
    fprintf(stderr,"Usage: %s -oOUTPUT INPUT [-nNAME]\n",argv[0]);
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
  int dstc=font_text_from_png_image(&dst,image,name,namec);
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
