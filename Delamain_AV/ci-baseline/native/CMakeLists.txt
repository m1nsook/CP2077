cmake_minimum_required(VERSION 3.25)
project(DelamainFlight VERSION 0.1.0 LANGUAGES CXX)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Use Windows x64 MSVC (Visual Studio 2022 or later)")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
set(RED4EXT_HEADER_ONLY ON CACHE BOOL "" FORCE)
set(RED4EXT_INSTALL OFF CACHE BOOL "" FORCE)
# The submodule revision recorded by DAV develop. Never silently use a
# different SDK revision. The source port handles the known body offset.
FetchContent_Declare(red4ext_sdk
  URL https://github.com/jackhumbert/RED4ext.SDK/archive/c28c7f63155c57eb636c8427396849d8abd47643.zip
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE)
FetchContent_MakeAvailable(red4ext_sdk)
add_library(DelamainFlight SHARED src/Flight.cpp)
target_compile_definitions(DelamainFlight PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
target_compile_options(DelamainFlight PRIVATE /utf-8)
target_link_libraries(DelamainFlight PRIVATE RED4ext::SDK)
set_target_properties(DelamainFlight PROPERTIES PREFIX "" OUTPUT_NAME "DelamainFlight")
