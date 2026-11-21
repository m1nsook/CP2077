#pragma once

// Internal native input helper for future MISO mods. Not a public Lua/RTTI API.
// The caller MUST verify a suitable game UI is active and the m1core overlay
// is closed before supplying gameplayContextConfirmed=true.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace m1core::input {

struct ClientPoint
{
    long x{};
    long y{};
};

enum class MouseButton { Left, Right };

enum class ClickResult
{
    Sent,
    NotAllowed,
    InvalidTarget,
    NotForeground,
    OutsideClient,
    CursorFailed,
    InjectionFailed
};

// Replaceable backend makes the exact click sequencer independently testable
// without ever touching the real cursor, keyboard or game process.
class MouseBackend
{
public:
    virtual ~MouseBackend() = default;
    virtual ClickResult Resolve(HWND target, ClientPoint location, POINT& screen) = 0;
    virtual bool MoveCursor(POINT screen) = 0;
    virtual UINT Inject(const INPUT* events, UINT count) = 0;
};

[[nodiscard]] ClickResult ClickClient(
    MouseBackend& backend, HWND target, ClientPoint location, MouseButton button,
    bool gameplayContextConfirmed);

// Windows implementation uses SetCursorPos + SendInput. This is an OS-level
// input attempt, NOT guaranteed to be accepted by Cyberpunk's in-game UI.
// Never call automatically or from an ImGui draw callback.
[[nodiscard]] ClickResult ClickGameClient(
    HWND target, ClientPoint location, MouseButton button,
    bool gameplayContextConfirmed);

} // namespace m1core::input
