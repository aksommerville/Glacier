#include "test/ma_test.h"

MA_ITEST(first_thing) {
  return 0;
}

  MA_ITEST ( second_thing ) { 
  return 0;
}

MA_ITEST(with_groups,abc,def) {
  MA_ASSERT_CALL(123,"negative results are failure")
  return 0;
}

XXX_MA_ITEST(with_groups_and_spaces , abc , def , xyz ) {
  return 0;
}
