#!/bin/sh

REPO_URL="https://github.com/DoozyX/clang-format-lint-action.git"
REPO_FOLDER="thirdparty/clang-format-lint-action"
COMMIT="c3b2c943e924028b93a707a5b1b017976ab8d50c"

if test ! -d "$REPO_FOLDER"; then
    git clone $REPO_URL "$REPO_FOLDER" || { exit 1; }
    (cd $REPO_FOLDER && git checkout "$COMMIT" && cd -) || { exit 1; }
else
    (cd "$REPO_FOLDER" && git fetch origin && git checkout "$COMMIT" && cd - )|| exit 1
fi
