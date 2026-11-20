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

// A system close command must never enter the overlay's keyboard capture.
inline bool IsSystemCloseCommand(UINT message, WPARAM command) noexcept
{
    return message == WM_SYSCOMMAND && (command & 0xFFF0u) == SC_CLOSE;
}

// Alt+F4 is a window-management command even when the user chose F4 as a
// menu key. The ALT context bit comes with WM_SYSKEYDOWN from Windows.
inline bool IsAltF4Command(UINT message, WPARAM key, LPARAM data) noexcept
{
    return message == WM_SYSKEYDOWN && key == VK_F4 &&
           (static_cast<unsigned long long>(data) & (1ull << 29)) != 0;
}

// Some games suppress ordinary WM_SYSKEYDOWN via RAWINPUT NOLEGACY.
// The user's Alt+F4 must still reach the native close path in that case.
inline bool IsRawAltF4(DWORD type, USHORT key, USHORT flags, bool altHeld) noexcept
{
    return type == RIM_TYPEKEYBOARD && key == VK_F4 &&
           (flags & RI_KEY_BREAK) == 0 && altHeld;
}

inline bool IsWindowCloseMessage(UINT message) noexcept
{
    return message == WM_CLOSE || message == WM_DESTROY ||
           message == WM_QUERYENDSESSION || message == WM_ENDSESSION;
}
} // namespace m1core
