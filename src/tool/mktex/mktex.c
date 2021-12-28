#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include "common/fs.h"
#include "common/png.h"
#include "common/encoder.h"
#include "mktex_tilesheet.h"

/* Generate the full C text from a decoded PNG image.
 * If conversion is necessary, we modify (image) in place.
 */
 
static int text_from_png_image(
  void *dstpp,
  struct png_image *image,
  const char *name,int namec,
  int tilesize
) {

  // Convert to 32-bit RGBA. Even if it had no alpha to begin with, single format saves us a lot of headache.
  if ((image->colortype!=6)||(image->depth!=8)) {
    struct png_image tmp={0};
    if (png_image_convert(&tmp,8,6,image)<0) {
      png_image_cleanup(&tmp);
      return -1;
    }
    png_image_cleanup(image);
    memcpy(image,&tmp,sizeof(struct png_image));
  }
  
  // Feed it into a tilesheet object.
  // Note it is "tilesheet" even for single images.
  struct mktex_tilesheet tilesheet={0};
  if (mktex_tilesheet_read_image(&tilesheet,image,name,namec,tilesize)<0) {
    mktex_tilesheet_cleanup(&tilesheet);
    return -1;
  }
  
  // One can imagine allowing customization of this step at the command line.
  if (mktex_tilesheet_optimize(&tilesheet)<0) {
    mktex_tilesheet_cleanup(&tilesheet);
    return -1;
  }
  
  struct encoder encoder={0};
  int err=mktex_tilesheet_encode(&encoder,&tilesheet);
  mktex_tilesheet_cleanup(&tilesheet);
  if (err<0) {
    encoder_cleanup(&encoder);
    return -1;
  }

  *(void**)dstpp=encoder.v; // HANDOFF
  return encoder.c;
}

/* Evaluate unsigned integer, <0 on error.
 */
 
static int uint_eval(const char *src,int srcc) {
  if (!src) return -1;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return -1;
  int v=0;
  for (;srcc-->0;src++) {
    int digit=(*src)-'0';
    if ((digit<0)||(digit>9)) return -1;
    if (v>INT_MAX/10) return -1;
    v*=10;
    if (v>INT_MAX-digit) return -1;
    v+=digit;
  }
  return v;
}

/* Validate name: Letters, digits, and underscore, and lead must not be a digit.
 */
 
static int is_c_identifier(const char *src,int srcc) {
  if (!src) return 0;
  if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  if (srcc<1) return 0;
  if ((src[0]>='0')&&(src[0]<='9')) return 0;
  for (;srcc-->0;src++) {
    if ((*src>='a')&&(*src<='z')) continue;
    if ((*src>='A')&&(*src<='Z')) continue;
    if ((*src>='0')&&(*src<='9')) continue;
    if (*src=='_') continue;
    return 0;
  }
  return 1;
}

/* Main.
 */
 
int main(int argc,char **argv) {

  const char *dstpath=0,*srcpath=0;
  int tilesize=0;
  int argp=1;
  for (;argp<argc;argp++) {
    if (!memcmp(argv[argp],"-o",2)) {
      if (dstpath) {
        fprintf(stderr,"%s: Multiple output paths\n",argv[0]);
        return 1;
      }
      dstpath=argv[argp]+2;
    } else if (!memcmp(argv[argp],"--tilesize=",11)) {
      if (tilesize) {
        fprintf(stderr,"%s: Multiple '--tilesize'\n",argv[0]);
        return 1;
      }
      if (!strcmp(argv[argp]+11,"auto")) {
        tilesize=-1;
      } else if ((tilesize=uint_eval(argv[argp]+11,-1))<1) {
        fprintf(stderr,"%s: Expected integer for tilesize, found '%s'\n",argv[0],argv[argp]+11);
        return 1;
      }
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
    fprintf(stderr,"Usage: %s -oOUTPUT INPUT [--tilesize=INT]\n",argv[0]);
    fprintf(stderr,"  OUTPUT is a C file, INPUT is a PNG file.\n");
    return 1;
  }
  
  int namec=0;
  const char *name=srcpath;
  int i=0; for (;srcpath[i];i++) if (srcpath[i]=='/') name=srcpath+i+1;
  while (name[namec]&&(name[namec]!='.')) namec++;
  
  // If tilesize was not provided at the command line, it may be a suffix on the file name, after a dash.
  if (!tilesize) {
    if ((namec>=5)&&!memcmp(name+namec-5,"-auto",5)) {
      tilesize=-1;
      namec-=5;
    } else {
      const char *tssrc=name+namec;
      int tssrcc=0;
      while ((tssrcc<namec)&&(tssrc[-1]>='0')&&(tssrc[-1]<='9')) { tssrc--; tssrcc++; }
      if ((tssrcc<namec)&&(tssrc[-1]=='-')) {
        int v=uint_eval(tssrc,tssrcc);
        if (v>0) {
          tilesize=v;
          namec-=tssrcc+1;
        }
      }
    }
  }
  
  if ((tilesize>0)&&(tilesize&3)) {
    fprintf(stderr,"%s: Invalid tile size %d, must be a multiple of 4.\n",srcpath,tilesize);
    return 1;
  }
  
  // Whatever's left of name must be a valid C identifier.
  if (!is_c_identifier(name,namec)) {
    fprintf(stderr,"%s: Invalid name '%.*s', must be a C identifier.\n",srcpath,namec,name);
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
  
  void *dst=0;
  int dstc=text_from_png_image(&dst,image,name,namec,tilesize);
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
