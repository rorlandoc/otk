# OTK - Abaqus ODB Toolkit
## High performance VTK Integration for Abaqus ODB files

### Introduction

OTK is a toolkit for abaqus ODB files. It is written in C++ and uses the VTK library for visualization. It is designed to be used as a library, but also provides a command line tool for quick visualization of ODB files.

### Requirements

OTK requires the following:

* Abaqus 2023
* VTK 9 or later
* CMake 3.16 or later
* A C++ compiler with C++20 support

### Building

OTK uses CMake as its build system. To build OTK, create a build directory and run CMake from it:

```bash
cmake -S. -Bbuild
cmake --build build
```

### License

OTK is licensed under the MIT license. See the [LICENSE](LICENSE) file for details.