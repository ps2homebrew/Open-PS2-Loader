#!/bin/bash

## Download lwNBD
REPO_URL="https://github.com/bignaux/lwNBD.git"
REPO_FOLDER="modules/network/lwNBD"
COMMIT="15f1c14536d662e2e00d38d0aa91ef24149e8b96"
if test ! -d "$REPO_FOLDER"; then
  git clone $REPO_URL "$REPO_FOLDER" || { exit 1; }
  (cd $REPO_FOLDER && git checkout "$COMMIT" && cd -) || { exit 1; }
else
  (cd "$REPO_FOLDER" && git fetch origin && git checkout "$COMMIT" && cd - )|| exit 1
fi
