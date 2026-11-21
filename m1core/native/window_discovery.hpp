#pragma once
#include <Windows.h>

namespace m1core {
// Discovery behavior adapted from src/window/window.cpp in the MIT-only
// CET 1.38.1 source provided by yamashi for m1core development.
// Original copyright (c) 2018 yamashi. See LICENSE-YAMASHI-MIT.
[[nodiscard]] HWND FindCyberpunkViewport() noexcept;
} // namespace m1core
