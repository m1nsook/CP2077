#include "ui_theme.hpp"

#include <imgui.h>
#include <system_error>

namespace m1core
{
namespace
{
ImFont* g_monoFont = nullptr;
ImFont* g_headingFont = nullptr;

constexpr ImVec4 Color(int r, int g, int b, int alpha = 255)
{
    return {r / 255.0F, g / 255.0F, b / 255.0F, alpha / 255.0F};
}

void Set(ImGuiCol key, ImVec4 color)
{
    ImGui::GetStyle().Colors[key] = color;
}

ImFont* AddExistingFont(const std::filesystem::path& file, float size)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec) || ec)
        return nullptr;
    return ImGui::GetIO().Fonts->AddFontFromFileTTF(file.string().c_str(), size);
}
} // namespace

void ApplyNativeTheme()
{
    // Solid shadcn-inspired zinc surfaces and semantic contrast. No fake
    // glass/transparency effects: ImDrawList renders the custom controls.
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 13.0F;
    style.ChildRounding = 11.0F;
    style.FrameRounding = 8.0F;
    style.PopupRounding = 9.0F;
    style.ScrollbarRounding = 8.0F;
    style.GrabRounding = 8.0F;
    style.TabRounding = 7.0F;
    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;
    style.PopupBorderSize = 1.0F;
    style.WindowPadding = {20.0F, 18.0F};
    style.FramePadding = {11.0F, 8.0F};
    style.CellPadding = {12.0F, 12.0F};
    style.ItemSpacing = {10.0F, 9.0F};
    style.ItemInnerSpacing = {8.0F, 6.0F};
    style.ScrollbarSize = 8.0F;
    style.GrabMinSize = 14.0F;
    style.IndentSpacing = 20.0F;

    Set(ImGuiCol_Text,              Color(250, 250, 250));
    Set(ImGuiCol_TextDisabled,      Color(161, 161, 170));
    Set(ImGuiCol_WindowBg,          Color(17, 17, 19));
    Set(ImGuiCol_ChildBg,           Color(21, 21, 24));
    Set(ImGuiCol_PopupBg,           Color(24, 24, 27));
    Set(ImGuiCol_Border,            Color(52, 52, 57));
    Set(ImGuiCol_BorderShadow,      Color(0, 0, 0, 0));
    Set(ImGuiCol_FrameBg,           Color(39, 39, 42));
    Set(ImGuiCol_FrameBgHovered,    Color(49, 49, 54));
    Set(ImGuiCol_FrameBgActive,     Color(55, 55, 60));
    Set(ImGuiCol_TitleBg,           Color(17, 17, 19));
    Set(ImGuiCol_TitleBgActive,     Color(17, 17, 19));
    Set(ImGuiCol_TitleBgCollapsed,  Color(17, 17, 19));
    Set(ImGuiCol_MenuBarBg,         Color(17, 17, 19));
    Set(ImGuiCol_ScrollbarBg,       Color(17, 17, 19));
    Set(ImGuiCol_ScrollbarGrab,     Color(52, 52, 57));
    Set(ImGuiCol_ScrollbarGrabHovered, Color(70, 70, 77));
    Set(ImGuiCol_ScrollbarGrabActive,  Color(96, 165, 250));
    Set(ImGuiCol_CheckMark,         Color(59, 130, 246));
    Set(ImGuiCol_SliderGrab,        Color(250, 250, 250));
    Set(ImGuiCol_SliderGrabActive,  Color(59, 130, 246));
    Set(ImGuiCol_Button,            Color(39, 39, 42));
    Set(ImGuiCol_ButtonHovered,     Color(53, 53, 59));
    Set(ImGuiCol_ButtonActive,      Color(65, 65, 71));
    Set(ImGuiCol_Header,            Color(43, 43, 48));
    Set(ImGuiCol_HeaderHovered,     Color(53, 53, 59));
    Set(ImGuiCol_HeaderActive,      Color(62, 62, 68));
    Set(ImGuiCol_Separator,         Color(52, 52, 57));
    Set(ImGuiCol_SeparatorHovered,  Color(96, 165, 250));
    Set(ImGuiCol_SeparatorActive,   Color(59, 130, 246));
    Set(ImGuiCol_ResizeGrip,        Color(52, 52, 57));
    Set(ImGuiCol_ResizeGripHovered, Color(96, 165, 250));
    Set(ImGuiCol_ResizeGripActive,  Color(59, 130, 246));
    Set(ImGuiCol_TableHeaderBg,     Color(24, 24, 27));
    Set(ImGuiCol_TableBorderStrong, Color(52, 52, 57));
    Set(ImGuiCol_TableBorderLight,  Color(43, 43, 47));
    Set(ImGuiCol_TableRowBg,        Color(24, 24, 27));
    Set(ImGuiCol_TableRowBgAlt,     Color(29, 29, 32));
    Set(ImGuiCol_TextSelectedBg,    Color(59, 130, 246, 90));
    Set(ImGuiCol_ModalWindowDimBg,  Color(0, 0, 0, 175));
}

void LoadOptionalFonts(const std::filesystem::path& overlayDirectory)
{
    ImGuiIO& io = ImGui::GetIO();
    g_monoFont = nullptr;
    g_headingFont = nullptr;
    // All development ZIPs include fonts from the OFL-licensed upstreams.
    // The runtime never fetches files from a CDN or alters system fonts.
    ImFont* fallback = io.Fonts->AddFontDefault();
    const auto fontDir = overlayDirectory / "fonts";
    ImFont* primary = AddExistingFont(fontDir / "Geist-Regular.ttf", 16.0F);
    g_headingFont = AddExistingFont(fontDir / "Geist-SemiBold.ttf", 22.0F);
    g_monoFont = AddExistingFont(fontDir / "JetBrainsMono-Regular.ttf", 14.0F);
    io.FontDefault = primary ? primary : fallback;
}

ImFont* HeadingFont() noexcept
{
    return g_headingFont;
}
ImFont* MonoFont() noexcept
{
    return g_monoFont;
}
} // namespace m1core
