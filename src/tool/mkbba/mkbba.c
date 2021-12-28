/* mkbba.c
 * Convert standard MIDI files to BBA's private format.
 * This file is just the outer glue.
 * Other files copied from bb with as little modification as I can manage.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "common/fs.h"

int mid2bba_convert(void *dstpp,const void *src,int srcc,const char *srcpath);

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
    fprintf(stderr,"Usage: %s -oOUTPUT INPUT\n",argv[0]);
    fprintf(stderr,"  OUTPUT is a BBA file (opaque binary), INPUT is a MIDI file.\n");
    return 1;
  }
  
  void *src=0;
  int srcc=file_read(&src,srcpath);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",srcpath);
    return 1;
  }
  
  void *dst=0;
  int dstc=mid2bba_convert(&dst,src,srcc,srcpath);
  free(src);
  if (dstc<0) {
    fprintf(stderr,"%s: Failed to convert song\n",srcpath);
    return 1;
  }
  
  int err=file_write(dstpath,dst,dstc);
  free(dst);
  if (err<0) {
    fprintf(stderr,"%s: Failed to write file\n",dstpath);
    return 1;
  }
  
  return 0;
}
