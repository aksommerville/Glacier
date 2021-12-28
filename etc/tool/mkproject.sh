#!/bin/sh

echo ""
echo "****************************************************"
echo "*          Multiarcade New Project Wizard          *"
echo "****************************************************"
echo ""

echo "Project name must be a valid C identifier."
echo "Recommend a leading uppercase letter."
while true ; do
  read -p "Project name: " PROJECT_NAME
  if [ -z "$PROJECT_NAME" ] ; then
    echo "Cancelled."
    exit 0
  fi
  if [ "${#PROJECT_NAME}" -gt 16 ] ; then
    echo "Limit 16 characters."
    continue
  fi
  if ! ( echo "$PROJECT_NAME" | grep -q '^[a-zA-Z_][0-9a-zA-Z_]*$' ) ; then
    echo "Not a C identifier."
    continue
  fi
  break
done
echo ""

DSTDIR="$(dirname $PWD)/$PROJECT_NAME/"
echo "Will create new project at '$DSTDIR'."
read -p "Is this OK? [Y/n] " OK
if [ "$OK" = "n" ] ; then
  echo "Cancelled."
  exit 0
fi
echo ""

mkdir "$DSTDIR" || exit 1
cp -r Makefile src etc .gitignore "$DSTDIR" || exit 1
echo "# $PROJECT_NAME" >"$DSTDIR/README.md"
git init "$DSTDIR" || exit 1
echo "$DSTDIR: Created new Multiarcade project."
