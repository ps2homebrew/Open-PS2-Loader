#!/bin/bash

## Download lwNBD
REPO_URL="https://github.com/bignaux/lwNBD.git"
REPO_FOLDER="modules/network/lwNBD"
BRANCH_NAME="b74564e7222b04b211c7c6bd04b3c4ed25be4e0b"
if test ! -d "$REPO_FOLDER"; then
  git clone --depth 1 -b $BRANCH_NAME $REPO_URL "$REPO_FOLDER" || exit 1
else
  (cd "$REPO_FOLDER" && git fetch origin && git reset --hard "origin/${BRANCH_NAME}" && git checkout "$BRANCH_NAME" && cd - )|| exit 1
fi
