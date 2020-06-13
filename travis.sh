#!/bin/bash

set -x # expand and echo commands
export XUO_TRAVIS=1

echo TRAVIS_BUILD_DIR is $TRAVIS_BUILD_DIR
echo TASK is $TASK
echo TRAVIS_TAG is $TRAVIS_TAG
echo TRAVIS_OS_NAME is $TRAVIS_OS_NAME

if [[ "$TASK" == "clang-format" ]]; then
    clang-format-7 --version
    echo Validating code formatting.
    clang-format-7 --style=file -i src/*.{h,cpp} src/*/*.{h,cpp} tools/*/*.{h,cpp} xuocore/*.{h,cpp} common/*.{h,cpp}
    dirty=$(git ls-files --modified)
    if [[ $dirty ]]; then
        echo Failed. The following files are incorrectly formatted:
        echo $dirty
        echo "Diff:"
        git --no-pager diff --no-color
        exit 1
    else
        echo Code formatting validation passed.
    fi
fi

if [[ "$TRAVIS_TAG" != "" ]]; then
    export EXTRA="-DXUO_DEPLOY=On -DXUO_VERSION=$TRAVIS_TAG"
    export BUILD=v$TRAVIS_TAG
else
    export BUILD=master
fi

if [[ "$TASK" == "clang" ]]; then
    echo Building Debug
    mkdir -p debug && cd debug && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . || exit 1
    cd ..
    echo Building Release
    mkdir -p release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release $EXTRA && cmake --build . || exit 1
    cd ..
    echo Building $BUILD package
    mkdir -p crossuo-linux-$BUILD
    cp release/src/crossuo.so crossuo-linux-$BUILD/
    cp release/src/crossuo crossuo-linux-$BUILD/
    cp release/tools/xuoi/xuolauncher crossuo-linux-$BUILD/
    tar -czvf crossuo-linux-$BUILD.tgz crossuo-linux-$BUILD/
fi

if [[ "$TASK" == "gcc" ]]; then
    sudo ln -s /usr/bin/gcc-7 /usr/local/bin/gcc
    sudo ln -s /usr/bin/g++-7 /usr/local/bin/g++
    export CC=/usr/bin/gcc-7
    export CXX=/usr/bin/g++-7
    gcc -v && g++ -v && cmake --version
    echo Building Debug
    mkdir -p debug && cd debug && cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . || exit 1
    #cd ..
    #echo Building Release
    #mkdir -p release && cd release && cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Release && cmake --build . || exit 1
fi

if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then
    brew update
    brew install ninja # sdl2 glew
    brew outdated cmake || brew upgrade cmake
    gcc -v && g++ -v && cmake --version
    echo Building Release
    mkdir -p release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release $EXTRA && cmake --build . || exit 1
    cd ..
    echo Building $BUILD package
    mkdir -p CrossUO.app/Contents/MacOS/
    cp release/src/crossuo.so CrossUO.app/Contents/MacOS/
    cp release/src/crossuo CrossUO.app/Contents/MacOS/
    cp release/tools/xuoi/xuolauncher CrossUO.app/Contents/MacOS/
    mkdir -p CrossUO.app/Contents/Resources/
    cp resources/crossuo.icns CrossUO.app/Contents/Resources/
    cp resources/Info.plist CrossUO.app/Contents/
    zip -r crossuo-osx-$BUILD.zip CrossUO.app
fi
