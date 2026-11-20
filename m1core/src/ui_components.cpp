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
    int colors = 0;
    if (tone == Tone::Primary) {
        // Standard ImGui button, visually distinct like the user's white Save action.
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.98F, 0.98F, 0.98F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85F, 0.85F, 0.88F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.76F, 0.76F, 0.79F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.05F, 0.05F, 0.06F, 1.0F));
        colors = 4;
    } else if (tone == Tone::Danger) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.31F, 0.12F, 0.14F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.42F, 0.17F, 0.19F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.72F, 0.74F, 1.0F));
        colors = 3;
    } else if (tone == Tone::Ghost) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.055F, 0.055F, 0.065F, 1.0F));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16F, 0.16F, 0.18F, 1.0F));
        colors = 2;
    }
    if (size.y <= 0.0F) size.y = 34.0F;
    const bool clicked = ImGui::Button(label, size);
    if (colors) ImGui::PopStyleColor(colors);
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
    const float width = ImGui::CalcTextSize(display.c_str()).x + 26.0F;
    // Native Selectable for input and keyboard navigation. The only custom
    // drawing is a cosmetic underline, not a custom hit area or widget.
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18F, 0.18F, 0.21F, 0.65F));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.22F, 0.22F, 0.25F, 0.85F));
    if (!selected)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.64F, 0.64F, 0.68F, 1.0F));
    const bool clicked = ImGui::Selectable(display.c_str(), selected,
                            ImGuiSelectableFlags_None, ImVec2(width, 42.0F));
    if (!selected) ImGui::PopStyleColor();
    ImGui::PopStyleColor(3);
    if (selected) {
        const ImVec2 a = ImGui::GetItemRectMin();
        const ImVec2 b = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(a.x+8.0F, b.y-1.5F), ImVec2(b.x-8.0F, b.y-1.5F),
            IM_COL32(250, 250, 250, 255), 2.0F);
    }
    return clicked;
}

bool SidebarItem(const char* label, const char* count, bool selected, float width)
{
    ImGui::PushID(label);
    // A real full-row ImGui Selectable; label and count are aligned on the
    // same baseline independently, rather than concatenated with padding.
    const bool clicked = ImGui::Selectable("##row", selected,
                              ImGuiSelectableFlags_None, ImVec2(width, 42.0F));
    const ImVec2 top = ImGui::GetItemRectMin();
    const ImVec2 bottom = ImGui::GetItemRectMax();
    const ImVec2 next = ImGui::GetCursorScreenPos();
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    const ImVec2 countSize = ImGui::CalcTextSize(count);
    const ImVec4 textColor = selected ? ImVec4(0.98F, 0.98F, 0.98F, 1.0F)
                                      : ImVec4(0.70F, 0.70F, 0.74F, 1.0F);

    ImGui::PushClipRect(ImVec2(top.x+12.0F, top.y),
                        ImVec2(bottom.x-countSize.x-22.0F, bottom.y), true);
    ImGui::SetCursorScreenPos(ImVec2(top.x+12.0F,
                        top.y+(42.0F-labelSize.y)*0.5F));
    ImGui::TextColored(textColor, "%s", label);
    ImGui::PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(bottom.x-countSize.x-12.0F,
                                      top.y+(42.0F-countSize.y)*0.5F));
    ImGui::TextDisabled("%s", count);
    ImGui::SetCursorScreenPos(next);
    ImGui::PopID();
    return clicked;
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
