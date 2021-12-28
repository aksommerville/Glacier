#include "test/ma_test.h"
#include "ma_itest_toc.h"

int main(int argc,char **argv) {
  const struct ma_itest *test=ma_itestv;
  int i=sizeof(ma_itestv)/sizeof(struct ma_itest);
  for (;i-->0;test++) {
    if (!ma_test_filter(argc,argv,test->name,test->file,test->tags,test->ignore)) {
      fprintf(stderr,"MA_TEST SKIP %s (%s) [%s:%d]\n",test->name,test->tags,test->file,test->line);
    } else if (test->fn()<0) {
      fprintf(stderr,"MA_TEST FAIL %s (%s) [%s:%d]\n",test->name,test->tags,test->file,test->line);
    } else {
      fprintf(stderr,"MA_TEST PASS %s (%s) [%s:%d]\n",test->name,test->tags,test->file,test->line);
    }
  }
  return 0;
}
