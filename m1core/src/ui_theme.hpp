#pragma once

#include <filesystem>

struct ImFont;

namespace m1core
{
// Native Dear ImGui styling based on m1core_mod_settings.html.
// This is not an embedded browser, HTML renderer or Tailwind runtime.
void ApplyNativeTheme();

// Fonts can be provided as loose files by the user, never downloaded at
// runtime. A Windows system font is used if Geist is not installed.
void LoadOptionalFonts(const std::filesystem::path& overlayDirectory);

// Package-provided Geist heading and JetBrains Mono fonts.
ImFont* HeadingFont() noexcept;
ImFont* MediumFont() noexcept;

// Optional mono typography for logs/technical values.
ImFont* MonoFont() noexcept;
} // namespace m1core
