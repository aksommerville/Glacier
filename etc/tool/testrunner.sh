#!/bin/bash

# Control verbosity right here:
log_before() { true ; }
log_loose()  { echo "$1" ; }
log_detail() { echo "$1" ; }
log_fail()   { echo -e "\x1b[91mFAIL\x1b[0m $1" ; }
log_pass()   { echo -e "\x1b[92mPASS\x1b[0m $1" ; }
log_skip()   { echo -e "\x1b[90mSKIP\x1b[0m $1" ; }

#-----------------------------------------

FAILC=0
PASSC=0
SKIPC=0

for EXE in $* ; do
  log_before "$EXE"
  while IFS= read INPUT ; do
    read INTRODUCER KEYWORD ARGS <<<"$INPUT"
    if [ "$INTRODUCER" = MA_TEST ] ; then
      case "$KEYWORD" in
        PASS)
          log_pass "$ARGS"
          PASSC=$((PASSC+1))
        ;;
        FAIL)
          log_fail "$ARGS"
          FAILC=$((FAILC+1))
        ;;
        SKIP)
          log_skip "$ARGS"
          SKIPC=$((SKIPC+1))
        ;;
        DETAIL)
          log_detail "$ARGS"
        ;;
        *)
          log_loose "$KEYWORD $ARGS"
        ;;
      esac
    else
      log_loose "$INPUT"
    fi
  done < <( "$EXE" $MA_TEST_FILTER 2>&1 || echo "MA_TEST FAIL $EXE" )
done

if [ "$FAILC" -gt 0 ] ; then
  echo -e "\x1b[41m    \x1b[0m $FAILC fail, $PASSC pass, $SKIPC skip"
  exit 1
elif [ "$PASSC" -gt 0 ] ; then
  echo -e "\x1b[42m    \x1b[0m $FAILC fail, $PASSC pass, $SKIPC skip"
else
  echo -e "\x1b[40m    \x1b[0m $FAILC fail, $PASSC pass, $SKIPC skip"
fi
