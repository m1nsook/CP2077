#pragma once

#include <imgui.h>
#include <string_view>
#include <vector>
#include <string>

namespace m1core::ui
{
// Native, shadcn-inspired primitives. Dear ImGui owns input/navigation;
// ImDrawList owns the visible chrome (not stock widget skins).
enum class Tone { Primary, Secondary, Ghost, Danger };

namespace color {
constexpr ImU32 Canvas = IM_COL32(9, 9, 11, 255);
constexpr ImU32 Surface = IM_COL32(12, 12, 14, 255);
constexpr ImU32 Card = IM_COL32(17, 17, 20, 255);
constexpr ImU32 Raised = IM_COL32(24, 24, 27, 255);
constexpr ImU32 Border = IM_COL32(52, 52, 57, 255);
constexpr ImU32 Foreground = IM_COL32(250, 250, 250, 255);
constexpr ImU32 Muted = IM_COL32(161, 161, 170, 255);
constexpr ImU32 Blue = IM_COL32(250, 250, 250, 255); // primary white, user's final.html
constexpr ImU32 BlueHover = IM_COL32(212, 212, 216, 255); // neutral focus ring
constexpr ImU32 Amber = IM_COL32(251, 191, 36, 255);
constexpr ImU32 Danger = IM_COL32(248, 113, 113, 255);
}

bool Button(const char* label, Tone tone = Tone::Secondary, ImVec2 size = {});
bool Tab(const char* label, bool selected);
bool NavTab(const char* label, const char* count, bool selected);
bool SidebarItem(const char* label, const char* count, bool selected, float width);
bool Switch(const char* id, bool enabled);
bool Slider(const char* id, double& value, double min, double max,
            double step, int decimals, float width);
bool Select(const char* id, int& selected, const std::vector<std::string>& choices,
            float width);
void Separator(float space = 10.0F);
void CountBadge(const char* count);
} // namespace m1core::ui
