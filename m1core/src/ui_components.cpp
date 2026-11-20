#include "ui_components.hpp"
#include "ui_components_math.hpp"

#include <algorithm>
#include <string>

namespace m1core::ui
{
// Built-in Dear ImGui widgets. No hand-drawn controls, invisible hit boxes,
// custom pointer math, or bespoke widget navigation.
bool Button(const char* label, Tone tone, ImVec2 size)
{
    if (tone == Tone::Danger) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.31F, 0.12F, 0.14F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42F, 0.17F, 0.19F, 1.0F));
    }
    const bool clicked = ImGui::Button(label, size);
    if (tone == Tone::Danger) ImGui::PopStyleColor(2);
    return clicked;
}

bool Tab(const char* label, bool selected)
{
    return ImGui::Selectable(label, selected, ImGuiSelectableFlags_None, ImVec2(0.0F, 32.0F));
}

bool NavTab(const char* label, const char* count, bool selected)
{
    std::string display(label);
    if (count && *count) { display += "  "; display += count; }
    const float width = ImGui::CalcTextSize(display.c_str()).x + 22.0F;
    return ImGui::Selectable(display.c_str(), selected, ImGuiSelectableFlags_None,
                             ImVec2(width, 34.0F));
}

bool SidebarItem(const char* label, const char* count, bool selected, float width)
{
    std::string display(label);
    if (count && *count) { display += "  "; display += count; }
    return ImGui::Selectable(display.c_str(), selected, ImGuiSelectableFlags_None,
                             ImVec2(width, 32.0F));
}

bool Switch(const char* id, bool enabled)
{
    bool value = enabled;
    return ImGui::Checkbox(id, &value);
}

bool Slider(const char* id, double& value, double min, double max,
            double step, int decimals, float width)
{
    if (max <= min) return false;
    ImGui::SetNextItemWidth((std::max)(90.0F, width));
    const char* format = decimals == 0 ? "%.0f" : "%.2f";
    const bool changed = ImGui::SliderScalar(id, ImGuiDataType_Double,
        &value, &min, &max, format, ImGuiSliderFlags_AlwaysClamp);
    if (changed && step > 0.0) {
        value = SnapSliderValue((value - min) / (max - min), min, max, step);
    }
    return changed;
}

bool Select(const char* id, int& selected, const std::vector<std::string>& choices,
            float width)
{
    ImGui::SetNextItemWidth((std::max)(110.0F, width));
    const char* preview = selected >= 0 && static_cast<std::size_t>(selected) < choices.size()
        ? choices[static_cast<std::size_t>(selected)].c_str() : "Select...";
    bool changed = false;
    if (ImGui::BeginCombo(id, preview)) {
        for (std::size_t i = 0; i < choices.size(); ++i) {
            if (ImGui::Selectable(choices[i].c_str(), selected == static_cast<int>(i))) {
                selected = static_cast<int>(i);
                changed = true;
            }
            if (selected == static_cast<int>(i)) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    return changed;
}

void Separator(float space)
{
    ImGui::Dummy(ImVec2(0, space * 0.5F));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, space * 0.5F));
}

void CountBadge(const char* count)
{
    ImGui::TextDisabled("%s", count);
}
} // namespace m1core::ui
