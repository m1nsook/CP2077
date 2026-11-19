#pragma once

#include <RED4ext/Api/v1/PluginHandle.hpp>

namespace RED4ext::v1 { struct Sdk; }

namespace m1core {

// Optional, process-local user32 detours used only by the opt-in overlay.
// If either detour fails, both are detached and raw-relative virtual mouse
// input remains available as the fallback.
bool InstallCursorDetours(RED4ext::v1::PluginHandle plugin, const RED4ext::v1::Sdk* sdk);

// Called on every overlay open/close and focus-loss transition.
// Does not require an ImGui context and is safe to call repeatedly.
void SetCursorCaptured(bool active);

// Restore the game's desired clipping rectangle before removing hooks.
void RemoveCursorDetours();

bool CursorDetoursAvailable();
} // namespace m1core
