#!/bin/bash

# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

set -x # expand and echo commands
export XUO_CI=1

export FULL_REF_NAME=$1
export BUILD_TYPE=$2
export COMPILER=$3

echo FULL_REF_NAME is $FULL_REF_NAME
echo BUILD_TYPE is $BUILD_TYPE
echo COMPILER is $COMPILER
echo GITHUB_REF_NAME is $GITHUB_REF_NAME
echo RUNNER_OS is $RUNNER_OS

if [[ "$BUILD_TYPE" != "Release" ]]; then
  echo "Skipping package"
  exit 0
fi

if [[ "$GITHUB_REF_NAME" != "" && "$GITHUB_REF_NAME" != "master" ]]; then
    export BUILD=v$GITHUB_REF_NAME
else
    git tag master -f
    export BUILD=master
fi

ls -la build/
ls -la build/src/
ls -la build/tools/xuoi/

if [[ "$RUNNER_OS" == "Linux" ]]; then
  if [[ "$COMPILER" == "clang" ]]; then
      pwd
      echo Building $BUILD package
      mkdir -p crossuo-linux-$BUILD
      cp build/src/libcrossuo.so crossuo-linux-$BUILD/crossuo.so
      cp build/src/crossuo crossuo-linux-$BUILD/
      cp build/tools/xuoi/xuolauncher crossuo-linux-$BUILD/
      tar -czvf crossuo-linux-$BUILD.tgz crossuo-linux-$BUILD/
  fi
elif [[ "$RUNNER_OS" == "macOS" ]]; then
    echo Building $BUILD package
    mkdir -p CrossUO.app/Contents/MacOS/
    cp build/src/crossuo.so CrossUO.app/Contents/MacOS/
    cp build/src/crossuo CrossUO.app/Contents/MacOS/
    cp build/tools/xuoi/xuolauncher CrossUO.app/Contents/MacOS/
    mkdir -p CrossUO.app/Contents/Resources/
    cp resources/crossuo.icns CrossUO.app/Contents/Resources/
    cp resources/Info.plist CrossUO.app/Contents/
    zip -r crossuo-osx-$BUILD.zip CrossUO.app
fi