#pragma once

#include <RED4ext/Api/v1/PluginHandle.hpp>
#include <Windows.h>

namespace RED4ext::v1 { struct Sdk; }

namespace m1core {

// Optional, process-local user32 detours used only by the opt-in overlay.
// If either detour fails, both are detached and raw-relative virtual mouse
// input remains available as the fallback.
bool InstallCursorDetours(RED4ext::v1::PluginHandle plugin, const RED4ext::v1::Sdk* sdk, HWND gameWindow);

// Called on every overlay open/close and focus-loss transition.
// Does not require an ImGui context and is safe to call repeatedly.
void SetCursorCaptured(bool active);

// Restore the game's desired clipping rectangle before removing hooks.
void RemoveCursorDetours();

// System pointer is the primary input mode when detours are installed.
// These never reposition the real cursor or constrain it to the game monitor.
bool CursorDetoursAvailable();
void EnsureSystemCursorVisible();
// Called from the game's WndProc subclass, so ShowCursor always executes on
// the game window's own thread (its visibility counter is per-thread).
bool HandleCursorVisibilityMessage(UINT message);
void RestoreSystemCursorOnWindowThread();
} // namespace m1core
