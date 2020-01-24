# Compiling

This project requires CMake 3.12 to build. It currently supports Visual Studio 2015/2017/2019, GCC 7.0 and Clang 7.

### Windows

#### Dependencies

The required dependencies for windows should be automatically downloaded by CMake.

#### Building

```bat
cmake -B build -S crossuo
cmake --open build
```

Or, you can use CMake-GUI to configure and generate the solution. For a step-by-step visual guide similar to our setup, please refer to [this](https://github.com/yuzu-emu/yuzu/wiki/Building-for-Windows) guide.

### Linux

#### Dependencies

* [CMake](https://cmake.org/)
* [SDL2](https://www.libsdl.org/download-2.0.php)
* [GLEW](http://glew.sourceforge.net/)
* [Ninja](https://ninja-build.org/) [optional, recommended]
* gcc or clang

#### Building

```bash
$ git clone https://github.com/crossuo/crossuo
$ cmake -B build -S crossuo
$ cmake --build build
$ cd build/src && ./crossuo
```

  > You'll need a `crossuo.cfg` in the working directory. Please use X:UO Launcher to configure the client.

### MacOSX

#### Dependencies

* [CMake](https://cmake.org/) (`brew install cmake`)
* [SDL2](https://www.libsdl.org/download-2.0.php) (`brew install sdl2`)
* [GLEW](http://glew.sourceforge.net/) (`brew install glew`)
* [Ninja](https://ninja-build.org/) (`brew install ninja`) [otpional]
* A recent version of Xcode and the Xcode command line tools

#### Building

```
$ git clone https://github.com/crossuo/crossuo
$ cmake -B build -S crossuo
$ cmake --build build --config Release
$ cd build/src && ./crossuo
```
  > You'll need a `crossuo.cfg` in the working directory. Please use X:UO Launcher to configure the client.

Then you should have an `crossuo` inside `build/crossuo`

### Raspberry PI

#### Dependencies

```
sudo apt-get install cmake libgles2-mesa-dev
```
#### Building

```
$ git clone https://github.com/crossuo/crossuo
$ cmake -B build -S crossuo
$ cmake --build build --config Release
$ cd build/src && ./crossuo
```
  > You'll need a `crossuo.cfg` in the working directory
  
Then you should have an `crossuo` inside `build/src`