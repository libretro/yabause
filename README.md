[![Last Release Status](https://github.com/FCare/Kronos/actions/workflows/release_on_tag.yml/badge.svg)

# Sega Saturn Emulated Hardware

You can read about the Sega Saturn Console and is components [here](https://www.copetti.org/writings/consoles/sega-saturn/).

# Building

## [CMake](https://cmake.org/)

The CMake system has been refined to build the full Yabause stack.
you have to download CMake and run it in the root of the first CMakeLists.txt. Currently in `/yabause/CMakeLists.txt`.

### Platforms

Since CMake is a true cross platform build tool you can build on plenty of platforms and easily add new ones.

Currently used are:

- Windows
- Linux (Debian)

### QT Version

You can set the CMake flag `YAB_USE_QT5` to build a Standalone version which requires you to have QT5 installed

### LibRetro - RetroArch Version

For this you need to se the CMake flag `KRONOS_LIBRETRO_CORE`. This will build a version of the core consuming LibRetro and there fore can be used with LibRetro compatible front-ends like RetroArch (which is what i test against)

#### Old make build script (obsolete)

The old build script is still there for reference. It contains custom build code for various platforms and is the base reference for newer entries in the CMake code. It is still though considered obsolete.

For retroarch core:

- mkdir build_retro; cd build_retro;
- make -j4 -C ../yabause/src/libretro/ generate-files
- make -j4 -C ../yabause/src/libretro/
- then execute retroarch -L ../yabause/src/libretro/kronos_libretro.so path_to_your_iso

# Pre-Build Versions

## Older Builds

Older Windows builds can be found [here](http://tradu-france.com/index.php?page=fullstory&id=676)!

# Platform Notes

## RetroArch

Is the Frontend which is used for manual tests

### Windows

Works quite well on a modern Hardware.

### Raspberry Pi 4 Raspberian OS

Has currently various problems running and is not usable.

- The Shaders are too complex and Mesa can not handle that. It will fail with a driver crash with Mesa 19.2
- Code is to demanding on the CPU by now even with frameskip 5 there is no acceptable framerate even on 2d games.

# Outdated Reference Documentation

- [Description](/yabause/blob/kronos-cmake_ci/yabause/README.DC)
- [Dreamcast](/yabause/blob/kronos-cmake_ci/yabause/README.DC)
- [Linux](/yabause/blob/kronos-cmake_ci/yabause/README.QT)
- [QT](/yabause/blob/kronos-cmake_ci/yabause/README.QT)
- [Windows](/yabause/blob/kronos-cmake_ci/yabause/README.WIN)

# Contributing

To generate a changelog, add in your commits the [ChangeLog] tag. Changelog will be extracted like this

  `git shortlog --grep=Changelog --since "01 Jan 2020"`
