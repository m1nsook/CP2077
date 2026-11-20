#pragma once

#include <RED4ext/Api/v1/PluginHandle.hpp>
#include <Windows.h>

namespace RED4ext::v1 { struct Sdk; }

namespace m1core {
// Legacy name retained to minimize API churn: initializes Cyberpunk's native
// engine ForceCursor service on the game HWND thread. Does NOT detour user32.
bool InstallCursorDetours(RED4ext::v1::PluginHandle plugin,
                          const RED4ext::v1::Sdk* sdk, HWND gameWindow);
bool CursorDetoursAvailable(); // Engine cursor service resolved, not Win32 hooks.
void SetCursorCaptured(bool active, bool focusLost = false);
void RemoveCursorDetours();
// Handles an m1core-owned registered Windows message and applies engine cursor
// state on the window thread; does not consume ordinary user input.
bool HandleCursorVisibilityMessage(UINT message);
void RestoreSystemCursorOnWindowThread();
void EnsureSystemCursorVisible(); // No-op: the engine controls system cursor.
} // namespace m1core
