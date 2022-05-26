#!/bin/bash

## Download languages
REPO_URL="https://github.com/ps2homebrew/Open-PS2-Loader-lang"
REPO_FOLDER="lng_src"
BRANCH_NAME="main"
if test ! -d "$REPO_FOLDER"; then
  git clone --depth 1 -b $BRANCH_NAME $REPO_URL "$REPO_FOLDER" || exit 1
else
  (cd "$REPO_FOLDER" && git fetch origin && git reset --hard "origin/${BRANCH_NAME}" && git checkout "$BRANCH_NAME" && cd - )|| exit 1
fi
