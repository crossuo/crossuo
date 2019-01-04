# Compiling

This project requires CMake 3.7 to build. It currently supports Visual Studio 2015, 2017, GCC 7.0 and Clang 7.

### Windows

#### Dependencies

The required dependencies for windows will be automatically downloaded by CMake.

#### Building

On Windows, to generate the Visual Studio solution, go inside your cloned OrionUO folder and do:

```bat
md build
cd build
cmake -G "Visual Studio 2017" ..
```

  > This will build a 32bits executable. If you do want 64bits, you can do `cmake -G "Visual Studio 2017 Win64" ..` instead. Note that plugins are still unsupported in the 64bits client.

Or, you can use CMake-GUI to configure and generate the solution. For a step-by-step visual guide similar to our setup, please refer to [this](https://github.com/yuzu-emu/yuzu/wiki/Building-for-Windows) guide.

### Linux

#### Dependencies

* [CMake](https://cmake.org/)
* [SDL2](https://www.libsdl.org/download-2.0.php)
* [GLEW](http://glew.sourceforge.net/)
* [Ninja](https://ninja-build.org/)
* gcc or clang

#### Building

```bash
$ git clone https://github.com/OrionUO/OrionUO
$ mkdir build
$ cd build
$ cmake -G Ninja ../OrionUO -DCMAKE_BUILD_TYPE=Release
$ ninja orion -j8
$ cd OrionUO
$ ./OrionUO
```

  > You'll need a `OrionUO.cfg` in the working directory. Please use Orion Launcher to configure the client.

### MacOSX

#### Dependencies

* [CMake](https://cmake.org/) (`brew install cmake`)
* [SDL2](https://www.libsdl.org/download-2.0.php) (`brew install sdl2`)
* [GLEW](http://glew.sourceforge.net/) (`brew install glew`)
* [Ninja](https://ninja-build.org/) (`brew install ninja`)
* A recent version of Xcode and the Xcode command line tools

#### Building

```
$ git clone https://github.com/OrionUO/OrionUO
$ mkdir build
$ cd build
$ cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
$ ninja orion -j8
$ cd OrionUO
$ ./OrionUO
```
  > You'll need a `OrionUO.cfg` in the working directory. Please use Orion Launcher to configure the client.

Then you should have an `OrionUO` inside `build/OrionUO`