#pragma once
#include <imgui.h>

namespace m1core {
// This pointer is owned by the current ImGui font atlas. It is cleared
// before atlas reload/destruction, not a separate renderer or font manager.
inline ImFont* g_LogFont = nullptr;
// Karla Regular remains the default ImGui font (and owns merged Phosphor
// glyphs); these extra weights are used for text-only headings/labels.
inline ImFont* g_KarlaMedium = nullptr;
inline ImFont* g_KarlaSemiBold = nullptr;
inline ImFont* g_KarlaBold = nullptr;
inline bool g_IconsAvailable = false;
inline void SetLogFont(ImFont* font) noexcept { g_LogFont = font; }
inline ImFont* LogFont() noexcept { return g_LogFont; }
inline void SetKarlaWeights(ImFont* medium, ImFont* semiBold, ImFont* bold) noexcept
{
    g_KarlaMedium = medium;
    g_KarlaSemiBold = semiBold;
    g_KarlaBold = bold;
}
inline ImFont* KarlaMedium() noexcept { return g_KarlaMedium; }
inline ImFont* KarlaSemiBold() noexcept { return g_KarlaSemiBold; }
inline ImFont* KarlaBold() noexcept { return g_KarlaBold; }
inline void SetIconsAvailable(bool value) noexcept { g_IconsAvailable = value; }
inline bool HasIcons() noexcept { return g_IconsAvailable; }
} // namespace m1core
