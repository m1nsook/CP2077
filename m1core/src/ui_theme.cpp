#include "ui_theme.hpp"

#include <imgui.h>
#include <system_error>

namespace m1core
{
namespace
{
ImFont* g_monoFont = nullptr;
ImFont* g_headingFont = nullptr;
ImFont* g_mediumFont = nullptr;

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
    // A restrained native ImGui theme. All controls use ImGui's built-in
    // interaction and navigation, while fonts, spacing and color are configurable.
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0F; // flat sharp M1CORE reference
    style.ChildRounding = 0.0F;
    style.FrameRounding = 0.0F;
    style.PopupRounding = 0.0F;
    style.ScrollbarRounding = 0.0F;
    style.GrabRounding = 1.0F;
    style.TabRounding = 7.0F;
    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;
    style.PopupBorderSize = 1.0F;
    style.WindowPadding = {20.0F, 18.0F};
    style.FramePadding = {11.0F, 8.0F};
    style.CellPadding = {14.0F, 13.0F};
    style.ItemSpacing = {11.0F, 10.0F};
    style.ItemInnerSpacing = {8.0F, 6.0F};
    style.ScrollbarSize = 9.0F;
    style.GrabMinSize = 16.0F;
    style.IndentSpacing = 20.0F;

    Set(ImGuiCol_Text,              Color(250, 250, 250));
    Set(ImGuiCol_TextDisabled,      Color(161, 161, 170));
    Set(ImGuiCol_WindowBg,          Color(3, 3, 3));
    Set(ImGuiCol_ChildBg,           Color(5, 5, 5));
    Set(ImGuiCol_PopupBg,           Color(9, 9, 9));
    Set(ImGuiCol_Border,            Color(64, 64, 64));
    Set(ImGuiCol_BorderShadow,      Color(0, 0, 0, 0));
    Set(ImGuiCol_FrameBg,           Color(14, 14, 14));
    Set(ImGuiCol_FrameBgHovered,    Color(32, 32, 32));
    Set(ImGuiCol_FrameBgActive,     Color(44, 44, 44));
    Set(ImGuiCol_TitleBg,           Color(17, 17, 19));
    Set(ImGuiCol_TitleBgActive,     Color(17, 17, 19));
    Set(ImGuiCol_TitleBgCollapsed,  Color(17, 17, 19));
    Set(ImGuiCol_MenuBarBg,         Color(17, 17, 19));
    Set(ImGuiCol_ScrollbarBg,       Color(17, 17, 19));
    Set(ImGuiCol_ScrollbarGrab,     Color(52, 52, 57));
    Set(ImGuiCol_ScrollbarGrabHovered, Color(70, 70, 77));
    Set(ImGuiCol_ScrollbarGrabActive,  Color(161, 161, 170));
    Set(ImGuiCol_CheckMark,         Color(250, 250, 250));
    Set(ImGuiCol_SliderGrab,        Color(250, 250, 250));
    Set(ImGuiCol_SliderGrabActive,  Color(250, 250, 250));
    Set(ImGuiCol_Button,            Color(27, 27, 27));
    Set(ImGuiCol_ButtonHovered,     Color(53, 53, 53));
    Set(ImGuiCol_ButtonActive,      Color(72, 72, 72));
    Set(ImGuiCol_Header,            Color(35, 35, 35));
    Set(ImGuiCol_HeaderHovered,     Color(55, 55, 55));
    Set(ImGuiCol_HeaderActive,      Color(70, 70, 70));
    Set(ImGuiCol_Separator,         Color(39, 39, 42));
    Set(ImGuiCol_SeparatorHovered,  Color(212, 212, 216));
    Set(ImGuiCol_SeparatorActive,   Color(250, 250, 250));
    Set(ImGuiCol_ResizeGrip,        Color(52, 52, 57));
    Set(ImGuiCol_ResizeGripHovered, Color(212, 212, 216));
    Set(ImGuiCol_ResizeGripActive,  Color(250, 250, 250));
    Set(ImGuiCol_TableHeaderBg,     Color(21, 21, 21));
    Set(ImGuiCol_TableBorderStrong, Color(52, 52, 57));
    Set(ImGuiCol_TableBorderLight,  Color(43, 43, 47));
    Set(ImGuiCol_TableRowBg,        Color(10, 10, 10));
    Set(ImGuiCol_TableRowBgAlt,     Color(21, 21, 21));
    Set(ImGuiCol_NavHighlight,      Color(212, 212, 216));
    Set(ImGuiCol_TextSelectedBg,    Color(161, 161, 170, 100));
    Set(ImGuiCol_ModalWindowDimBg,  Color(0, 0, 0, 175));
}

void LoadOptionalFonts(const std::filesystem::path& overlayDirectory)
{
    // Never resolve assets relative to Cyberpunk's working directory: the
    // player's Visual Studio skeleton used "assets/..." and lost its fonts.
    // In the game ZIP they live beside m1core.dll under the fonts folder.
    ImGuiIO& io = ImGui::GetIO();
    g_monoFont = g_headingFont = g_mediumFont = nullptr;
    ImFont* fallback = io.Fonts->AddFontDefault();
    const auto dir = overlayDirectory / "fonts";
    ImFont* regular = AddExistingFont(dir / "Geist-Regular.ttf", 16.0F);
    // FA solid is merged into the primary Geist atlas to render the user's
    // original nav/search/status glyphs. No runtime downloads.
    if (regular) {
        std::error_code ec;
        const auto fa = dir / "fa-solid-900.ttf";
        if (std::filesystem::is_regular_file(fa, ec) && !ec) {
            ImFontConfig config{};
            config.MergeMode = true;
            config.GlyphMinAdvanceX = 17.0F;
            static constexpr ImWchar iconRanges[] = {0xF000, 0xF8FF, 0};
            io.Fonts->AddFontFromFileTTF(fa.string().c_str(), 15.0F, &config, iconRanges);
        }
    }
    g_mediumFont = AddExistingFont(dir / "Geist-Medium.ttf", 16.0F);
    g_headingFont = AddExistingFont(dir / "Geist-SemiBold.ttf", 23.0F);
    g_monoFont = AddExistingFont(dir / "JetBrainsMono-Regular.ttf", 14.0F);
    io.FontDefault = regular ? regular : fallback;
}

ImFont* HeadingFont() noexcept { return g_headingFont; }
ImFont* MediumFont() noexcept { return g_mediumFont; }
ImFont* MonoFont() noexcept { return g_monoFont; }
} // namespace m1core
