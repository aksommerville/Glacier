#!/bin/sh

if [ "$#" -lt 1 ] ; then
  echo "Usage: $0 [INPUTS...] OUTPUT"
  exit 1
fi

TMPTOC=mid/itesttmp
rm -f "$TMPTOC"
touch "$TMPTOC"

while [ "$#" -gt 1 ] ; do
  SRCPATH="$1"
  shift 1
  SRCBASE="$(basename $SRCPATH)"
  nl -ba -s' ' "$SRCPATH" \
    | sed -En 's/^ *([0-9]+) *(XXX_)?MA_ITEST *\( *([0-9a-zA-Z_]+) *(, *([^)]*))?\).*$/'"$SRCBASE"' \1 _\2 \3 \5/p' \
    >"$TMPTOC"
done

DSTPATH="$1"
rm -f "$DSTPATH"

cat - >"$DSTPATH" <<EOF
#ifndef MA_ITEST_TOC_H
#define MA_ITEST_TOC_H

EOF

while read F L I N T ; do
  echo "int $N();" >>"$DSTPATH"
done <"$TMPTOC"

cat - >>"$DSTPATH" <<EOF

static const struct ma_itest {
  int (*fn)();
  const char *name;
  const char *file;
  int line;
  const char *tags;
  int ignore;
} ma_itestv[]={
EOF

while read F L I N T ; do
  if [ "$I" = "_XXX_" ] ; then
    IGNORE=1
  else
    IGNORE=0
  fi
  echo "  {$N,\"$N\",\"$F\",$L,\"$T\",$IGNORE}," >>"$DSTPATH"
done <"$TMPTOC"

cat - >>"$DSTPATH" <<EOF
};

#endif
EOF

rm "$TMPTOC"
