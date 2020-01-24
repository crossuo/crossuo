# Compiling

This project requires CMake 3.12 to build. It currently supports Visual Studio 2015/2017/2019, GCC 7+ and Clang 7+.

### Dependencies

Some platforms may require extra dependencies, we try to avoid them to simplify the building process, but in some cases this can be difficult, here are the instructions on how to get these dependencies for the platforms that require them.

#### Raspberry PI

```
sudo apt-get install cmake libgles2-mesa-dev
```

### Building

#### Linux, OSX or RaspberryPi

```bash
$ git clone https://github.com/crossuo/crossuo
$ cmake -B build -S crossuo
$ cmake --build build
$ cd build/src && ./crossuo
```

#### Windows

```bat
cmake -B build -S crossuo
cmake --open build
```

Or, you can use CMake-GUI to configure and generate the solution. For a step-by-step visual guide similar to our setup, please refer to [this](https://github.com/yuzu-emu/yuzu/wiki/Building-for-Windows) guide.


### Running

After building, you should have a few executables somewhre in the `build` directory. You'll need at least the `crossuo` and `xuolauncher`, copy them somewhere and you're ready to go.

 > You'll need all the original Ultima Online data files, which you can download from their official website or from your preferred free shard.
 
 > You'll also need a `crossuo.cfg` in the working directory to point to the data files and some other settings. This can be achieved by using X:UO Launcher to correctly configure and launch the client.
