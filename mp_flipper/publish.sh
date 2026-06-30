#!/usr/bin/env bash

readonly BRANCH="${1}"
readonly TARGET='./temp'
readonly REMOTE='git@github.com:ofabel/mp-flipper.git'

set -e

rm -rf ${TARGET}

git init -b ${BRANCH} ${TARGET} && cd ${TARGET} && git remote add origin ${REMOTE} && cd ..

rm -rf ${TARGET}/*

cp -r dist/pages/* ${TARGET}
touch ${TARGET}/.nojekyll

cd ${TARGET}

git add . && git commit -m "update docs" && git push origin ${BRANCH} --force || cd .

cd ..

rm -rf ${TARGET}

exit 0
