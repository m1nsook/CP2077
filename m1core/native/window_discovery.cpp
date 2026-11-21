#include "window_discovery.hpp"

#include <cwchar>

namespace m1core {
namespace {

// Adapted from the donated CET src/window/window.cpp. A known Win32
// game window can be located before a valid DX12 frame is available.
// Do not start a detached search thread or intercept any input here.
// A passive HWND WndProc will be installed by the RED4ext render bridge.
BOOL CALLBACK FindViewport(HWND candidate, LPARAM result) noexcept
{
    DWORD owner = 0;
    GetWindowThreadProcessId(candidate, &owner);
    if (owner != GetCurrentProcessId()) return TRUE;

    wchar_t className[128]{};
    if (!RealGetWindowClassW(candidate, className, 128) ||
        std::wcscmp(className, L"W2ViewportClass") != 0)
        return TRUE;

    *reinterpret_cast<HWND*>(result) = candidate;
    return FALSE;
}
} // namespace

HWND FindCyberpunkViewport() noexcept
{
    HWND target = nullptr;
    EnumWindows(&FindViewport, reinterpret_cast<LPARAM>(&target));
    return target;
}
} // namespace m1core
