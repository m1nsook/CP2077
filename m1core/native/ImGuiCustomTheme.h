#pragma once
#include "imgui.h"

// Original user-supplied ImGuiCustomTheme.h. Only standard ImGuiStyle
// colors and metrics; every widget remains an unmodified Dear ImGui widget.
inline void m1CoreTheme(ImGuiStyle& style = ImGui::GetStyle())
{
    ImVec4* colors = style.Colors;

    // ImGuiCol_ Color Tokens
    colors[ImGuiCol_WindowBg] = ImVec4(0.059f, 0.055f, 0.063f, 0.96f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.098f, 0.098f, 0.141f, 0.45f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.071f, 0.071f, 0.071f, 0.97f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.086f, 0.075f, 0.106f, 1.0f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.106f, 0.09f, 0.129f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.024f, 0.835f, 0.996f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.122f, 0.122f, 0.122f, 0.72f);
    colors[ImGuiCol_Text] = ImVec4(0.898f, 0.91f, 1.0f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.573f, 0.584f, 0.69f, 0.65f);
    colors[ImGuiCol_Border] = ImVec4(0.878f, 0.878f, 0.878f, 0.06f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.078f, 0.086f, 0.102f, 0.38f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.192f, 0.22f, 0.5f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.196f, 0.224f, 0.302f, 0.18f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.024f, 0.835f, 0.996f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.024f, 0.835f, 0.996f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.024f, 0.835f, 0.996f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.149f, 0.149f, 0.149f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.212f, 0.212f, 0.212f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(1.0f, 1.0f, 1.0f, 0.07f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.09f);
    colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    colors[ImGuiCol_Separator] = ImVec4(0.878f, 0.878f, 0.878f, 0.059f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.878f, 0.878f, 0.878f, 0.15f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.878f, 0.878f, 0.878f, 0.26f);
    colors[ImGuiCol_Tab] = ImVec4(0.141f, 0.137f, 0.161f, 0.31f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.878f, 0.878f, 0.878f, 0.08f);
    colors[ImGuiCol_TabActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.09f, 0.95f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.02f, 0.02f, 0.02f, 0.949f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.038f, 0.043f, 0.054f, 1.0f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.49f, 0.49f, 0.49f, 0.3f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.545f, 0.584f, 0.647f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.545f, 0.584f, 0.647f, 0.74f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.545f, 0.584f, 0.647f, 0.541f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.545f, 0.584f, 0.647f, 0.741f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.545f, 0.584f, 0.647f, 0.741f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.024f, 0.835f, 0.996f, 1.0f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.149f, 0.141f, 0.18f, 1.0f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.537f, 0.463f, 0.835f, 1.0f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.878f, 0.878f, 0.878f, 0.059f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.878f, 0.878f, 0.878f, 0.059f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.055f, 0.051f, 0.063f, 0.059f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.055f, 0.051f, 0.063f, 0.059f);

    // ImGuiStyleVar / Style Metrics
    style.WindowPadding = ImVec2(9.0f, 9.0f);
    style.FramePadding = ImVec2(9.0f, 9.0f);
    style.CellPadding = ImVec2(10.0f, 10.0f);
    style.ItemSpacing = ImVec2(15.0f, 15.0f);
    style.ItemInnerSpacing = ImVec2(8.5f, 8.5f);
    style.IndentSpacing = 12.0f;
    style.ScrollbarSize = 6.0f;
    style.GrabMinSize = 10.0f;
    style.WindowRounding = 7.0f;
    style.ChildRounding = 7.5f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 7.0f;
    style.ScrollbarRounding = 3.5f;
    style.GrabRounding = 3.5f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 2.0f;
    style.PopupBorderSize = 2.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
}
