#pragma once
#include <imgui.h>

namespace m1core::ui
{
// Title-bar text is dark on the user's cyan setup/keybind headers and on
// the lighter main-window title bar. Only push this while Begin() draws
// the title; regular window body text keeps the established dark theme.
inline ImVec4 TitleText() noexcept { return {0.055f, 0.065f, 0.08f, 1.0f}; }

// m1core's warning/unsaved/empty-bind state: amber, never vivid red.
inline ImVec4 WarningText() noexcept { return {1.0f, 0.65f, 0.20f, 1.0f}; }
} // namespace m1core::ui
