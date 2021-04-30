# Tessa

Tesselates a WKT polygon and writes the result in a simple-to-read text format.

## Usage

```
bin/tessa --help
```

For example:
```
bin/tessa -v --mesh data/test.wkt
```

## Building (linux and mac)

(If you have never compiled any C++, you may need to install `build-essentials`, for example with `apt install build-essentials` or `brew install build-essentials`.)

We depend on the following libaries:
* CGAL (geometric algorithms)
* Boost (parser)
* CLI11 (commandline argument parser)
* spdlog (logging)

You could get them on your own, but we recommend using [vcpkg](https://github.com/microsoft/vcpkg) as package manager for C++.
Install it first (I've put mine in ~/vcpkg), then in its directory:
```
./vcpkg install cgal
./vcpkg install cli11
./vcpkg install spdlog
```
Don't be surprised if this takes half an hour. Note when using Ubuntu: `sudo apt-get install yasm` and `sudo apt-get install autoconf` might be nessecary.

Next we need to configure the build files for Tessa.
Go into the `build` directory and run CMAKE.
It needs to know where your vcpkg is; substitute the folder where you have installed `vcpkg`.
Do not forget the `..` at the end of the line.
```
cd build
cmake -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake ..
```
If you get errors from CMAKE and try to fix it, you should probably `rm -r *` in the `build` directory, because it caches a bunch of things and sometimes confuses itself.

If the previous step succeeds, you can now actually build Tessa.
In the `build` directory, run:
```
make
```

(Depending on how your CMAKE is configured, maybe it's `ninja` instead of `make`.)

If you change the content of any of the C++ code, you only need to run `make` - not the `cmake` thing again.

## Building (Windows)

Install vcpkg just like on Linux and install the libraries.

```
./vcpkg install cgal
./vcpkg install cli11
./vcpkg install spdlog
```

Recent versions of Visual Studio and VSCode understand CMAKE directories, so it should probably just work if you open the Tessa directory.