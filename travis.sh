#!/bin/bash

export XUO_TRAVIS=1

if [[ "$TASK" == "clang-format" ]]; then
	clang-format-7 --version
	echo Validating code formatting.
	clang-format-7 --style=file -i src/*.{h,cpp} src/*/*.{h,cpp} installer/*.{h,cpp}
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
	mkdir debug && cd debug && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug && ninja crossuo_unity -j8 && ninja xuodump_unity -j8 && ninja xuo -j8 || exit 1
	cd ..
	echo Building Release
	mkdir release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release && ninja crossuo_unity -j8 && ninja xuodump_unity -j8 && ninja xuo -j8 || exit 1
	echo Make the zip file
	cd src
	zip CrossUO-Unbuntu-nightly.zip crossuo.so crossuo
	mv CrossUO-Unbuntu-nightly.zip ../../
fi

if [[ "$TASK" == "gcc" ]]; then
	echo Building Debug
	mkdir debug && cd debug && cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Debug && make -j8 || exit 1
	#cd ..
	#echo Building Release
	#mkdir release && cd release && cmake -G "Unix Makefiles" .. -DCMAKE_BUILD_TYPE=Release && make -j8 || exit 1
fi

if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then
	brew update
	brew install sdl2 ninja glew
	brew outdated cmake || brew upgrade cmake
	#echo Building Debug
	#mkdir debug && cd debug && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug && ninja crossuo_unity -j8 && ninja xuo -j8 || exit 1
	#cd ..
	echo Building Release
	mkdir release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release && ninja crossuo_unity -j8 && ninja xuo -j8 || exit 1
	echo Make the zip file
	cd src
	zip CrossUO-OSX-nightly.zip crossuo.so crossuo
	mv CrossUO-OSX-nightly.zip ../../
fi;
