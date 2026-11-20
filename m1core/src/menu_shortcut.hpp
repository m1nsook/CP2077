#pragma once

// Small platform-facing routing rules. Pure helpers, independently tested,
// so the live game WndProc never swallows a menu-key release or OS quit chord.
#include <Windows.h>

namespace m1core
{
// Only a *bare* key down for the player's selected shortcut is ours.
// Releasing a key must always reach the game; Alt+F4 (or any modifier+key)
// belongs to Windows/Cyberpunk, not m1core's single-key menu binding.
inline bool ConsumeMenuKey(UINT message, WPARAM key, int assignedKey,
                           bool recording, bool anyModifier) noexcept
{
    return assignedKey > 0 && !recording && !anyModifier &&
           static_cast<int>(key) == assignedKey &&
           (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
}

inline bool MayToggleMenu(bool focused, bool recording,
                          bool down, bool previouslyDown,
                          bool anyModifier) noexcept
{
    return focused && !recording && down && !previouslyDown && !anyModifier;
}

inline bool IsWindowCloseMessage(UINT message) noexcept
{
    return message == WM_CLOSE || message == WM_DESTROY ||
           message == WM_QUERYENDSESSION || message == WM_ENDSESSION;
}
} // namespace m1core
