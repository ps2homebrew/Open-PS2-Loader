#!/bin/bash

## Download lwNBD
REPO_URL="https://github.com/bignaux/lwNBD.git"
REPO_FOLDER="modules/network/lwNBD"
COMMIT="b74564e7222b04b211c7c6bd04b3c4ed25be4e0b"
if test ! -d "$REPO_FOLDER"; then
  git clone $REPO_URL "$REPO_FOLDER" || { exit 1; }
  (cd $REPO_FOLDER && git checkout $COMMIT && cd -) || { exit 1; }
fi
