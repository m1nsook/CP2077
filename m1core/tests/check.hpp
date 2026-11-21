#pragma once
#include <cstdlib>
#include <iostream>

// Always evaluate test checks, including MSVC /O2 /DNDEBUG Release builds.
// Using assert() here previously allowed every test to pass without running.
#define REQUIRE(condition) do { \
    if (!(condition)) { \
        std::cerr << __FILE__ << ":" << __LINE__ \
                  << ": failed: " #condition << '\n'; \
        std::exit(EXIT_FAILURE); \
    } \
} while (false)
