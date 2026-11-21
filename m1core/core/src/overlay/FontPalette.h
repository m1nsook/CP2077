#pragma once
#include <imgui.h>

namespace m1core {
// This pointer is owned by the current ImGui font atlas. It is cleared
// before atlas reload/destruction, not a separate renderer or font manager.
inline ImFont* g_LogFont = nullptr;
inline bool g_IconsAvailable = false;
inline void SetLogFont(ImFont* font) noexcept { g_LogFont = font; }
inline ImFont* LogFont() noexcept { return g_LogFont; }
inline void SetIconsAvailable(bool value) noexcept { g_IconsAvailable = value; }
inline bool HasIcons() noexcept { return g_IconsAvailable; }
} // namespace m1core
