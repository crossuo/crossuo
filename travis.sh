#!/bin/bash

export XUO_TRAVIS=1

echo TRAVIS_BUILD_DIR is $TRAVIS_BUILD_DIR
echo TASK is $TASK
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

if [[ "$TASK" == "clang" ]]; then
    echo Building Debug
    mkdir -p debug && cd debug && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . || exit 1
    cd ..
    echo Building Release
    mkdir -p release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release && cmake --build . || exit 1
    cd ..
    echo Building nightly package
    mkdir -p crossuo-linux-nightly
    cp release/src/crossuo.so crossuo-linux-nightly/
    cp release/src/crossuo crossuo-linux-nightly/
    cp release/tools/xuoi/xuolauncher crossuo-linux-nightly/
    tar -czvf crossuo-linux-nightly.tgz crossuo-linux-nightly/
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
    mkdir -p release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release && cmake --build . || exit 1
    cd ..
    echo Building nightly package
    mkdir -p crossuo-osx-nightly/CrossUO.app/MacOS
    cp release/src/crossuo.so crossuo-osx-nightly/CrossUO.app/MacOS/
    cp release/src/crossuo crossuo-osx-nightly/CrossUO.app/MacOS/
    cp release/tools/xuoi/xuolauncher crossuo-osx-nightly/CrossUO.app/MacOS/
    mkdir -p crossuo-osx-nightly/CrossUO.app/Resources
    cp resources/crossuo.icns crossuo-osx-nightly/CrossUO.app/Resources
    cp resources/Info.plist crossuo-osx-nightly/CrossUO.app
    zip crossuo-osx-nightly.zip crossuo-osx-nightly/*
fi
