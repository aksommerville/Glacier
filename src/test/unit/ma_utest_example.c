#include "test/ma_test.h"

static int ignored() {
  MA_ASSERT_STRINGS("apple",-1,"orange",-1)
  return 0;
}

static int some_unit_test() {
  return 0;
}

static int thing_number_b() {
  MA_ASSERT_NOT(1==2,"one should not equal two")
  MA_ASSERT(3,"three should be true")
  MA_ASSERT_INTS_OP(123,<,456)
  return 0;
}

/* TOC.
 */
 
int main(int argc,char **argv) {
  XXX_MA_UTEST(ignored,tag1,tag2)
  MA_UTEST(some_unit_test)
  MA_UTEST(thing_number_b)
  return 0;
}
