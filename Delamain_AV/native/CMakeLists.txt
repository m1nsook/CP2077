cmake_minimum_required(VERSION 3.25)
project(DelamainAV VERSION 0.2.16 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(RED4EXT_HEADER_ONLY ON CACHE BOOL "" FORCE)
set(RED4EXT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(red4ext_sdk
  GIT_REPOSITORY https://github.com/wopss/RED4ext.SDK.git
  GIT_TAG a34f0ad0a9e4f37cbe1fe001e4ff68c2949d0665)
FetchContent_MakeAvailable(red4ext_sdk)

add_library(DelamainAV SHARED
    src/physics.cpp
    src/diagnostics.hpp
    version.h
    DelamainAV.rc
)

target_include_directories(DelamainAV PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(DelamainAV PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    $<$<CONFIG:Debug>:DELAMAINAV_DEV_BUILD>
    $<$<CONFIG:RelWithDebInfo>:DELAMAINAV_DEV_BUILD>
)

# /utf-8 is a C++ compiler flag; rc.exe doesn't understand it.
target_compile_options(DelamainAV PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)

target_link_libraries(DelamainAV PRIVATE RED4ext::SDK version)

set_target_properties(DelamainAV PROPERTIES
    PREFIX ""
    OUTPUT_NAME "DelamainAV")