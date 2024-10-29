#!/bin/bash

## Protect reentrancy of this script if possible
if [ "x$1" != "xlocked" ]; then
  if command -v flock > /dev/null; then
    flock "$0" "$BASH" "$0" locked
    exit $?
  fi
  if command -v perl > /dev/null; then
    perl -MFcntl=:flock -e '$|=1; $f=shift; open(FH,$f) || die($!); flock(FH,LOCK_EX); my $exit_value = system(@ARGV); flock(FH,LOCK_UN); exit($exit_value);' "$0" "$BASH" "$0" locked
    exit $?
  fi
fi

## Download lwNBD
LWNBD_REPO_URL="https://github.com/bignaux/lwNBD.git"
LWNBD_REPO_FOLDER="modules/network/lwNBD"
LWNBD_COMMIT="9777a10f840679ef89b1ec6a588e2d93803d7c37"
if test ! -d "$LWNBD_REPO_FOLDER"; then
  git clone $LWNBD_REPO_URL "$LWNBD_REPO_FOLDER" || { exit 1; }
  (cd $LWNBD_REPO_FOLDER && git checkout "$LWNBD_COMMIT" && cd -) || { exit 1; }
else
  (cd "$LWNBD_REPO_FOLDER" && git fetch origin && git checkout "$LWNBD_COMMIT" && cd - )|| exit 1
fi

## Download lwNBD
LZ4_REPO_URL="https://github.com/lz4/lz4.git"
LZ4_REPO_FOLDER="modules/isofs/lz4"
LZ4_BRANCH_NAME="r116"
if test ! -d "$LZ4_REPO_FOLDER"; then
  git clone -b $LZ4_BRANCH_NAME $LZ4_REPO_URL "$LZ4_REPO_FOLDER" || { exit 1; }
  (cd $LZ4_REPO_FOLDER && git checkout "$LZ4_BRANCH_NAME" && cd - ) || { exit 1; }
else
  (cd "$LZ4_REPO_FOLDER" && git fetch origin && git reset --hard "${LZ4_BRANCH_NAME}" && git checkout "$LZ4_BRANCH_NAME" && cd - )|| exit 1
fi

# Port by JoseAaronLopezGarcia.

pushd modules/isofs/lz4
sed -i -e 's|#include <stdlib.h>   ||' lz4.c
sed -i -e 's|static const int LZ4_minLength = (MFLIMIT+1);||' lz4.c
sed -i '303i#if 0' lz4.c
sed -i '710i#endif' lz4.c
sed -i '852i#if 0' lz4.c
sed -i '872i#endif' lz4.c
sed -i '884d' lz4.c
sed -i '883d' lz4.c
popd