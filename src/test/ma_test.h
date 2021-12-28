/* ma_test.h
 */
 
#ifndef MA_TEST_H
#define MA_TEST_H

#include "multiarcade.h"
#include <stdio.h>
#include <string.h>

/* Identify tests.
 *********************************************************/
 
/* At global scope, MA_ITEST(name,...groups), followed directly by the function body.
 */
#define MA_ITEST(name,...) int name()
#define XXX_MA_ITEST(name,...) int name()

/* In the unit test's main(), MA_UTEST(name,...groups), naming a local function.
 */
#define MA_UTEST(name,...) _MA_UTEST(name,0,__VA_ARGS__)
#define XXX_MA_UTEST(name,...) _MA_UTEST(name,1,__VA_ARGS__)
#define _MA_UTEST(name,ignore,...) { \
  if (!ma_test_filter(argc,argv,#name,ma_basename(__FILE__),#__VA_ARGS__,ignore)) { \
    fprintf(stderr,"MA_TEST SKIP %s (%s) [%s]\n",#name,#__VA_ARGS__,ma_basename(__FILE__)); \
  } else if (name()<0) { \
    fprintf(stderr,"MA_TEST FAIL %s (%s) [%s]\n",#name,#__VA_ARGS__,ma_basename(__FILE__)); \
  } else { \
    fprintf(stderr,"MA_TEST PASS %s (%s) [%s]\n",#name,#__VA_ARGS__,ma_basename(__FILE__)); \
  } \
}

int ma_test_filter(int argc,char **argv,const char *name,const char *file,const char *tags,int ignore);
const char *ma_basename(const char *path);
int ma_string_printable(const char *src,int srcc);

/* Assertion helpers.
 ***********************************************************/
 
// Redefine as needed, eg if you have to return null on failures.
#define MA_FAILURE_RESULT -1
 
#define MA_FAIL_MORE(key,fmt,...) { \
  fprintf(stderr,"MA_TEST DETAIL * %15s: "fmt"\n",key,##__VA_ARGS__); \
}
 
#define MA_FAIL_BEGIN(fmt,...) { \
  fprintf(stderr,"MA_TEST DETAIL *******************************************\n"); \
  fprintf(stderr,"MA_TEST DETAIL * ASSERTION FAILED\n"); \
  fprintf(stderr,"MA_TEST DETAIL *------------------------------------------\n"); \
  MA_FAIL_MORE("Location","%s:%d",__FILE__,__LINE__) \
  if (fmt[0]) MA_FAIL_MORE("Message",fmt,##__VA_ARGS__) \
}

#define MA_FAIL_END { \
  fprintf(stderr,"MA_TEST DETAIL *******************************************\n"); \
  return MA_FAILURE_RESULT; \
}

/* Assertions.
 ***********************************************************/
 
#define MA_ASSERT(condition,...) if (!(condition)) { \
  MA_FAIL_BEGIN(""__VA_ARGS__) \
  MA_FAIL_MORE("Expected","true") \
  MA_FAIL_MORE("As written","%s",#condition) \
  MA_FAIL_END \
}
 
#define MA_ASSERT_NOT(condition,...) if (condition) { \
  MA_FAIL_BEGIN(""__VA_ARGS__) \
  MA_FAIL_MORE("Expected","false") \
  MA_FAIL_MORE("As written","%s",#condition) \
  MA_FAIL_END \
}

#define MA_ASSERT_CALL(call,...) { \
  int _result=(call); \
  if (_result<0) { \
    MA_FAIL_BEGIN(""__VA_ARGS__) \
    MA_FAIL_MORE("Expected","Successful call") \
    MA_FAIL_MORE("Result","%d",_result) \
    MA_FAIL_MORE("As written","%s",#call) \
    MA_FAIL_END \
  } \
}

#define MA_ASSERT_FAILURE(call,...) { \
  int _result=(call); \
  if (_result>=0) { \
    MA_FAIL_BEGIN(""__VA_ARGS__) \
    MA_FAIL_MORE("Expected","Failed call") \
    MA_FAIL_MORE("Result","%d",_result) \
    MA_FAIL_MORE("As written","%s",#call) \
    MA_FAIL_END \
  } \
}

#define MA_ASSERT_INTS_OP(a,op,b,...) { \
  int _a=(int)(a),_b=(int)(b); \
  if (!(_a op _b)) { \
    MA_FAIL_BEGIN(""__VA_ARGS__) \
    MA_FAIL_MORE("As written","%s %s %s",#a,#op,#b) \
    MA_FAIL_MORE("Values","%d %s %d",_a,#op,_b) \
    MA_FAIL_END \
  } \
}

#define MA_ASSERT_INTS(a,b,...) MA_ASSERT_INTS_OP(a,==,b,##__VA_ARGS__)

#define MA_ASSERT_STRINGS(a,ac,b,bc,...) { \
  const char *_a=(char*)(a),*_b=(char*)(b); \
  int _ac=(int)(ac),_bc=(int)(bc); \
  if (!_a) _ac=0; else if (_ac<0) { _ac=0; while (_a[_ac]) _ac++; } \
  if (!_b) _bc=0; else if (_bc<0) { _bc=0; while (_b[_bc]) _bc++; } \
  if ((_ac!=_bc)||memcmp(_a,_b,_ac)) { \
    MA_FAIL_BEGIN(""__VA_ARGS__) \
    MA_FAIL_MORE("(a) As written","%s : %s",#a,#ac) \
    MA_FAIL_MORE("(b) As written","%s : %s",#b,#bc) \
    if (ma_string_printable(_a,_ac)) MA_FAIL_MORE("(a) Value","%.*s",_ac,_a) \
    if (ma_string_printable(_b,_bc)) MA_FAIL_MORE("(b) Value","%.*s",_bc,_b) \
    MA_FAIL_END \
  } \
}

#endif
