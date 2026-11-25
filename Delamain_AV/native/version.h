#ifndef DELAMAINAV_VERSION_H
#define DELAMAINAV_VERSION_H

// Single source of truth for DelamainAV's version.
// Consumed by DelamainAV.rc (VERSIONINFO), physics.cpp's SESSION log line
// and Query()'s reported plugin version.

#define DELAMAINAV_VERSION_MAJOR 0
#define DELAMAINAV_VERSION_MINOR 2
#define DELAMAINAV_VERSION_PATCH 17
#define DELAMAINAV_VERSION_BUILD 24

#ifdef DELAMAINAV_DEV_BUILD
#  define DELAMAINAV_VERSION_STRING "0.2.17-dev.24"
#else
#  define DELAMAINAV_VERSION_STRING "0.2.17.24"
#endif

#endif // DELAMAINAV_VERSION_H
