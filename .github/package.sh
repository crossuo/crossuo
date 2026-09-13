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

# The in-client assistant (XUO_ASSISTANT, the CMake default is ON) builds the
# client as ONE self-contained crossuo executable. XUO_ASSISTANT=OFF keeps the
# exe+so layout instead, so the package needs the loader exe + the game
# library (libcrossuo.so) + its crossuo.so copy - shipping just the loader
# would be a client that cannot run the game.
ASSISTANT=${XUO_ASSISTANT:-ON}

if [[ "$RUNNER_OS" == "Linux" ]]; then
  if [[ "$COMPILER" == "clang" ]]; then
      pwd
      echo Building $BUILD package
      mkdir -p crossuo-linux-$BUILD
      cp build/src/crossuo crossuo-linux-$BUILD/
      if [[ "$ASSISTANT" == "OFF" ]]; then
          # exe+so layout: the loader needs the game library and its
          # crossuo.so copy next to it
          cp build/src/libcrossuo.so crossuo-linux-$BUILD/crossuo.so
          cp build/tools/xuoi/xuolauncher crossuo-linux-$BUILD/
      fi
      tar -czvf crossuo-linux-$BUILD.tgz crossuo-linux-$BUILD/
  fi
elif [[ "$RUNNER_OS" == "macOS" ]]; then
    echo Building $BUILD package
    mkdir -p CrossUO.app/Contents/MacOS/
    cp build/src/crossuo CrossUO.app/Contents/MacOS/
    if [[ "$ASSISTANT" == "OFF" ]]; then
        # exe+so layout: the loader needs the game library and its
        # crossuo.so copy next to it
        cp build/src/libcrossuo.so build/src/crossuo.so CrossUO.app/Contents/MacOS/
        cp build/tools/xuoi/xuolauncher CrossUO.app/Contents/MacOS/
    fi
    mkdir -p CrossUO.app/Contents/Resources/
    cp resources/crossuo.icns CrossUO.app/Contents/Resources/
    cp resources/Info.plist CrossUO.app/Contents/
    zip -r crossuo-osx-$BUILD.zip CrossUO.app
fi
