#ifndef DELAMAINAV_VERSION_H
#define DELAMAINAV_VERSION_H

// Single source of truth for DelamainAV's version.
// Consumed by DelamainAV.rc (VERSIONINFO), physics.cpp's SESSION log line
// and Query()'s reported plugin version.

#define DELAMAINAV_VERSION_MAJOR 0
#define DELAMAINAV_VERSION_MINOR 2
#define DELAMAINAV_VERSION_PATCH 16
#define DELAMAINAV_VERSION_BUILD 23

// Suffix is added automatically by the CMake config below.
#ifdef DELAMAINAV_DEV_BUILD
#  define DELAMAINAV_VERSION_STRING "0.2.16-dev.23"
#else
#  define DELAMAINAV_VERSION_STRING "0.2.16.23"
#endif

#endif // DELAMAINAV_VERSION_H