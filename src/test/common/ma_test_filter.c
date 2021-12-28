#include "test/ma_test.h"

/* Check one string against the filter list.
 */
 
static int ma_test_filter_single(int argc,char **argv,const char *q,int qc) {
  if (qc<0) { qc=0; while (q[qc]) qc++; }
  for (;argc-->0;argv++) {
    if (memcmp(*argv,q,qc)) continue;
    if ((*argv)[qc]) continue;
    return 1;
  }
  return 0;
}

/* Check a comma-delimited list against the filter list.
 */
 
static int ma_test_filter_multi(int argc,char **argv,const char *src) {
  int srcp=0;
  while (src[srcp]) {
    if (src[srcp]==',') { srcp++; continue; }
    if ((unsigned char)src[srcp]<=0x20) { srcp++; continue; }
    const char *q=src+srcp;
    int qc=0;
    while (src[srcp]&&(src[srcp]!=',')) { srcp++; qc++; }
    while (qc&&((unsigned char)q[qc-1]<=0x20)) qc--;
    if (ma_test_filter_single(argc,argv,q,qc)) return 1;
  }
  return 0;
}

/* Check all test filters.
 */
 
int ma_test_filter(int argc,char **argv,const char *name,const char *file,const char *tags,int ignore) {

  // Args should be provided as main() gets them, so skip the first one.
  if (argc>0) { argc--; argv++; }
  
  // No filter? Use the test case's default.
  if (!argc) return !ignore;
  
  // Filter present, so ignore (ignore) and use the filter exclusively.
  if (ma_test_filter_single(argc,argv,name,-1)) return 1;
  if (ma_test_filter_single(argc,argv,file,-1)) return 1;
  if (ma_test_filter_multi(argc,argv,tags)) return 1;
  
  return 0;
}

/* Base name.
 */
 
const char *ma_basename(const char *path) {
  const char *base=path;
  int i=0; for (;path[i];i++) {
    if (path[i]=='/') base=path+i+1;
  }
  return base;
}

/* Should we log this string?
 */
 
int ma_string_printable(const char *src,int srcc) {
  if (srcc<0) return 0;
  if (!src) return 0;
  if (srcc>200) return 0;
  for (;srcc-->0;src++) {
    if (*src<0x20) return 0;
    if (*src>0x7e) return 0;
  }
  return 1;
}
