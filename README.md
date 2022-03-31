# Spreadsheet

This is 'C++ Development Foundations: Black Belt' course project Spreadsheet.

Course: [Course](https://www.coursera.org/learn/c-plus-plus-black)

Spreadsheet is a spreadsheet engine(similar to Google Spreadsheet and Microsoft Excel).

The spreadsheet interface is described by the ```ISheet``` class from  `common.h`.

Spreadsheet uses [ANTLR4](https://www.antlr.org/) to Parse a cell formula.

## Installation

#### Ubuntu & Debian

Install necessary packages:

``` sh
$ sudo apt-get install cmake ninja-build pkgconf uuid-dev
```

Building:

``` sh
$ mkdir build && cd build
$ cmake .. -Wno-dev -DCMAKE_BUILD_TYPE=Debug -G Ninja -DWITH_LIBCXX=Off
$ ninja
```

#### Windows, Visual Studio 2017 
Building:

``` sh
mkdir build && cd build
cmake .. -G "Visual Studio 15 2017" -A x64 -Wno-dev 
```
Open file `build/spreadsheet.sln`.

