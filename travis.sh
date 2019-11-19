#!/bin/bash

export XUO_TRAVIS=1

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
	mkdir debug && cd debug && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_FORMAT=Off && cmake --build . || exit 1
	cd ..
	echo Building Release
	mkdir release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release -DENABLE_FORMAT=Off && cmake --build . || exit 1
	cd ..
	echo Building nightly package
	mkdir crossuo-ubuntu-nightly
	cd crossuo-ubuntu-nightly
	cp build/src/crossuo.so crossuo-ubuntu-nightly/
	cp build/src/crossuo crossuo-ubuntu-nightly/
	cp build/tools/xuoi/xuolauncher crossuo-ubuntu-nightly/
	tar -czvf crossuo-ubuntu-nightly.tgz crossuo-ubuntu-nightly/
fi

if [[ "$TASK" == "gcc" ]]; then
	echo Building Debug
	mkdir debug && cd debug && cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_FORMAT=Off && cmake --build . || exit 1
	#cd ..
	#echo Building Release
	#mkdir release && cd release && cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Release -DENABLE_FORMAT=Off && cmake --build .|| exit 1
fi

if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then
	##brew update
	##brew install sdl2 ninja glew
	#brew outdated cmake || brew upgrade cmake
	#echo Building Release
	#mkdir release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release -DENABLE_FORMAT=Off && ninja gfxtest -j8 || exit 1
	#cd ..
	#echo Building nightly package
	#mkdir crossuo-osx-nightly
	#cd crossuo-osx-nightly
	#cp build/src/crossuo.so crossuo-osx-nightly/
	#cp build/src/crossuo crossuo-osx-nightly/
	#cp build/tools/xuoi/xuolauncher crossuo-osx-nightly/
	#zip crossuo-osx-nightly.zip crossuo-osx-nightly/*
	echo "build skipped until glew is removed"
fi
