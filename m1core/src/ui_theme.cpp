#include "ui_theme.hpp"

#include <imgui.h>
#include <system_error>

namespace m1core
{
namespace
{
ImFont* g_monoFont = nullptr;

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
    // Colors and proportions are mapped from the user's supplied HTML:
    // #09090b canvas, #0f1014 surface, #14161c elevated, #1b1e26 hover,
    // #38bdf8 accent, #f59e0b warning, #fafafa primary and #a1a1aa muted.
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0F;
    style.ChildRounding = 10.0F;
    style.FrameRounding = 7.0F;
    style.PopupRounding = 9.0F;
    style.ScrollbarRounding = 12.0F;
    style.GrabRounding = 9.0F;
    style.TabRounding = 8.0F;
    style.WindowBorderSize = 1.0F;
    style.ChildBorderSize = 1.0F;
    style.FrameBorderSize = 1.0F;
    style.PopupBorderSize = 1.0F;
    style.WindowPadding = {16.0F, 15.0F};
    style.FramePadding = {10.0F, 7.0F};
    style.CellPadding = {10.0F, 8.0F};
    style.ItemSpacing = {9.0F, 8.0F};
    style.ItemInnerSpacing = {8.0F, 5.0F};
    style.ScrollbarSize = 9.0F;
    style.GrabMinSize = 16.0F;
    style.IndentSpacing = 19.0F;

    Set(ImGuiCol_Text,             Color(250, 250, 250));
    Set(ImGuiCol_TextDisabled,     Color(161, 161, 170));
    Set(ImGuiCol_WindowBg,         Color(15, 16, 20, 246));
    Set(ImGuiCol_ChildBg,          Color(11, 12, 16, 228));
    Set(ImGuiCol_PopupBg,          Color(20, 22, 28, 250));
    Set(ImGuiCol_Border,           Color(255, 255, 255, 22));
    Set(ImGuiCol_BorderShadow,     Color(0, 0, 0, 0));
    Set(ImGuiCol_FrameBg,          Color(20, 22, 28));
    Set(ImGuiCol_FrameBgHovered,   Color(27, 30, 38));
    Set(ImGuiCol_FrameBgActive,    Color(30, 38, 47));
    Set(ImGuiCol_TitleBg,          Color(9, 9, 11));
    Set(ImGuiCol_TitleBgActive,    Color(15, 16, 20));
    Set(ImGuiCol_TitleBgCollapsed, Color(9, 9, 11, 220));
    Set(ImGuiCol_MenuBarBg,        Color(11, 12, 15));
    Set(ImGuiCol_ScrollbarBg,      Color(9, 9, 11, 180));
    Set(ImGuiCol_ScrollbarGrab,    Color(255, 255, 255, 31));
    Set(ImGuiCol_ScrollbarGrabHovered, Color(255, 255, 255, 59));
    Set(ImGuiCol_ScrollbarGrabActive,  Color(56, 189, 248, 120));
    Set(ImGuiCol_CheckMark,        Color(56, 189, 248));
    Set(ImGuiCol_SliderGrab,       Color(250, 250, 250));
    Set(ImGuiCol_SliderGrabActive, Color(56, 189, 248));
    Set(ImGuiCol_Button,           Color(255, 255, 255, 12));
    Set(ImGuiCol_ButtonHovered,    Color(56, 189, 248, 30));
    Set(ImGuiCol_ButtonActive,     Color(56, 189, 248, 62));
    Set(ImGuiCol_Header,           Color(56, 189, 248, 29));
    Set(ImGuiCol_HeaderHovered,    Color(56, 189, 248, 46));
    Set(ImGuiCol_HeaderActive,     Color(56, 189, 248, 70));
    Set(ImGuiCol_Separator,        Color(255, 255, 255, 21));
    Set(ImGuiCol_SeparatorHovered, Color(56, 189, 248, 90));
    Set(ImGuiCol_SeparatorActive,  Color(56, 189, 248, 145));
    Set(ImGuiCol_ResizeGrip,       Color(56, 189, 248, 45));
    Set(ImGuiCol_ResizeGripHovered,Color(56, 189, 248, 100));
    Set(ImGuiCol_ResizeGripActive, Color(56, 189, 248, 190));
    Set(ImGuiCol_TableHeaderBg,    Color(20, 22, 28));
    Set(ImGuiCol_TableBorderStrong,Color(255, 255, 255, 32));
    Set(ImGuiCol_TableBorderLight, Color(255, 255, 255, 16));
    Set(ImGuiCol_TableRowBg,       Color(15, 16, 20, 110));
    Set(ImGuiCol_TableRowBgAlt,    Color(255, 255, 255, 4));
    Set(ImGuiCol_TextSelectedBg,   Color(56, 189, 248, 70));
    Set(ImGuiCol_ModalWindowDimBg, Color(0, 0, 0, 160));
}

void LoadOptionalFonts(const std::filesystem::path& overlayDirectory)
{
    ImGuiIO& io = ImGui::GetIO();
    g_monoFont = nullptr;
    const ImFont* defaultFont = io.Fonts->AddFontDefault();
    const auto fontDir = overlayDirectory / "fonts";

    ImFont* primary = AddExistingFont(fontDir / "Geist-Regular.ttf", 16.0F);
    if (!primary)
        primary = AddExistingFont(L"C:\\Windows\\Fonts\\segoeui.ttf", 16.0F);
    io.FontDefault = primary ? primary : const_cast<ImFont*>(defaultFont);
    g_monoFont = AddExistingFont(fontDir / "JetBrainsMono-Regular.ttf", 14.0F);
}

ImFont* MonoFont() noexcept
{
    return g_monoFont;
}
} // namespace m1core
