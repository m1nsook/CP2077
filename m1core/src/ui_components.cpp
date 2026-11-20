#include "ui_components.hpp"
#include "ui_components_math.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace m1core::ui
{
namespace
{
constexpr float kRadius = 8.0F;

bool Focused()
{
    return ImGui::IsItemFocused();
}
void FocusRing(ImDrawList* draw, const ImVec2& a, const ImVec2& b, float radius)
{
    if (Focused())
        draw->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1),
                      color::BlueHover, radius + 1.0F, 0, 1.4F);
}
void TextCentered(ImDrawList* draw, const ImVec2& a, const ImVec2& b,
                  const char* text, ImU32 tint)
{
    const ImVec2 t = ImGui::CalcTextSize(text);
    draw->AddText(ImVec2(a.x + (b.x-a.x-t.x)*0.5F,
                         a.y + (b.y-a.y-t.y)*0.5F), tint, text);
}
} // namespace

bool Button(const char* label, Tone tone, ImVec2 size)
{
    const ImVec2 t = ImGui::CalcTextSize(label);
    if (size.x <= 0) size.x = t.x + 25.0F;
    if (size.y <= 0) size.y = 33.0F;
    const bool clicked = ImGui::InvisibleButton(label, size);
    const bool hover = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    ImU32 fill = color::Raised;
    ImU32 fg = color::Foreground;
    ImU32 border = color::Border;
    if (tone == Tone::Primary) {
        fill = held ? IM_COL32(29, 78, 216, 255) :
               hover ? color::BlueHover : color::Blue;
        fg = color::Foreground;
        border = fill;
    } else if (tone == Tone::Ghost) {
        fill = held ? IM_COL32(45, 45, 50, 255) :
               hover ? color::Raised : color::Surface;
        border = tone == Tone::Ghost ? fill : border;
    } else if (tone == Tone::Danger) {
        fill = held ? IM_COL32(98, 28, 33, 255) :
               hover ? IM_COL32(72, 30, 36, 255) : IM_COL32(54, 28, 33, 255);
        border = IM_COL32(110, 43, 52, 255);
        fg = color::Danger;
    } else if (hover) {
        fill = IM_COL32(53, 53, 59, 255);
    }
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(a, b, fill, kRadius);
    if (tone != Tone::Ghost)
        draw->AddRect(a, b, border, kRadius);
    FocusRing(draw, a, b, kRadius);
    TextCentered(draw, a, b, label, fg);
    return clicked;
}

bool Tab(const char* label, bool selected)
{
    const ImVec2 text = ImGui::CalcTextSize(label);
    const ImVec2 size(text.x + 25.0F, 38.0F);
    const bool clicked = ImGui::InvisibleButton(label, size);
    const auto a = ImGui::GetItemRectMin();
    const auto b = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    if (ImGui::IsItemHovered())
        draw->AddRectFilled(a, b, IM_COL32(255, 255, 255, 7), 5.0F);
    TextCentered(draw, a, b, label, selected ? color::Foreground : color::Muted);
    if (selected)
        draw->AddLine(ImVec2(a.x + 12.0F, b.y - 1.0F),
                      ImVec2(b.x - 12.0F, b.y - 1.0F),
                      color::Foreground, 2.0F);
    FocusRing(draw, a, b, 5.0F);
    return clicked;
}

bool SidebarItem(const char* label, const char* count, bool selected, float width)
{
    const bool clicked = ImGui::InvisibleButton(label, ImVec2(width, 43.0F));
    const auto a = ImGui::GetItemRectMin();
    const auto b = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    if (selected || ImGui::IsItemHovered())
        draw->AddRectFilled(a, b, selected ? color::Raised : color::Card, 7.0F);
    if (selected)
        draw->AddRectFilled(ImVec2(a.x, a.y+9), ImVec2(a.x+2.0F, b.y-9),
                            color::Blue, 1.0F);
    FocusRing(draw, a, b, 7.0F);
    const ImVec2 countSize = ImGui::CalcTextSize(count);
    const float textEnd = b.x - countSize.x - 18.0F;
    draw->PushClipRect(ImVec2(a.x+12.0F, a.y), ImVec2(textEnd-8, b.y), true);
    draw->AddText(ImVec2(a.x+13.0F, a.y+13.0F),
                  selected ? color::Foreground : color::Muted, label);
    draw->PopClipRect();
    draw->AddText(ImVec2(b.x-countSize.x-12.0F, a.y+13.0F),
                  color::Muted, count);
    return clicked;
}

bool Switch(const char* id, bool enabled)
{
    constexpr float width = 42.0F, height = 24.0F;
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
    const auto a = ImGui::GetItemRectMin();
    const auto b = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(a, b, enabled ? color::Blue : color::Raised, height*0.5F);
    draw->AddRect(a, b, enabled ? color::Blue : color::Border, height*0.5F);
    draw->AddCircleFilled(ImVec2(a.x+(enabled ? width-12 : 12), a.y+12),
                          9.0F, color::Foreground);
    FocusRing(draw, a, b, height*0.5F);
    return clicked;
}

bool Slider(const char* id, double& value, double min, double max,
            double step, int decimals, float width)
{
    width = (std::max)(160.0F, width);
    constexpr float height = 35.0F;
    constexpr float valueArea = 62.0F;
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
    const auto a = ImGui::GetItemRectMin();
    const auto b = ImGui::GetItemRectMax();
    const float left = a.x + 9.0F;
    const float right = (std::max)(left+8.0F, b.x - valueArea - 9.0F);
    const bool valid = max > min;
    bool changed = false;
    if (valid && (pressed || (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)))) {
        const double t = std::clamp((ImGui::GetIO().MousePos.x-left) /
                                    static_cast<double>(right-left), 0.0, 1.0);
        const double result = SnapSliderValue(t, min, max, step);
        if (std::abs(result-value) > 0.000001) {
            value = result;
            changed = true;
        }
    }
    if (valid && ImGui::IsItemFocused()) {
        double delta = 0.0;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) delta = -step;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) delta = step;
        if (delta != 0) {
            const double result = std::clamp(value+delta, min, max);
            if (std::abs(result-value) > 0.000001) { value = result; changed=true; }
        }
    }

    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 trackA(left, a.y+16.0F), trackB(right, a.y+21.0F);
    draw->AddRectFilled(trackA, trackB, color::Raised, 3.0F);
    const float t = valid ? static_cast<float>(std::clamp((value-min)/(max-min), 0.0, 1.0)) : 0;
    const float pos = left+(right-left)*t;
    if (pos > left)
        draw->AddRectFilled(trackA, ImVec2(pos, trackB.y), color::Blue, 3.0F);
    draw->AddCircleFilled(ImVec2(pos, a.y+18.5F), 8.0F, color::Foreground);
    draw->AddCircle(ImVec2(pos, a.y+18.5F), 8.0F, color::Blue, 0, 2.0F);

    const ImVec2 chipA(b.x-valueArea+2.0F, a.y+1.0F), chipB(b.x, b.y-1.0F);
    draw->AddRectFilled(chipA, chipB, color::Raised, 7.0F);
    draw->AddRect(chipA, chipB, color::Border, 7.0F);
    char formatted[64]{};
    std::snprintf(formatted, sizeof(formatted), "%.*f", decimals, value);
    TextCentered(draw, chipA, chipB, formatted, color::Foreground);
    FocusRing(draw, a, b, 7.0F);
    return changed;
}

bool Select(const char* id, int& selected, const std::vector<std::string>& choices,
            float width)
{
    width = (std::max)(135.0F, width);
    const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, 35.0F));
    const auto a = ImGui::GetItemRectMin();
    const auto b = ImGui::GetItemRectMax();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(a, b, color::Raised, kRadius);
    draw->AddRect(a, b, ImGui::IsItemHovered() ? color::Blue : color::Border, kRadius);
    FocusRing(draw, a, b, kRadius);
    const char* preview = (selected >= 0 &&
        static_cast<std::size_t>(selected) < choices.size())
        ? choices[static_cast<std::size_t>(selected)].c_str() : "Select...";
    draw->PushClipRect(ImVec2(a.x+11, a.y), ImVec2(b.x-30, b.y), true);
    draw->AddText(ImVec2(a.x+12, a.y+9), color::Foreground, preview);
    draw->PopClipRect();
    draw->AddLine(ImVec2(b.x-19, a.y+14), ImVec2(b.x-15, a.y+18),
                  color::Muted, 1.7F);
    draw->AddLine(ImVec2(b.x-15, a.y+18), ImVec2(b.x-11, a.y+14),
                  color::Muted, 1.7F);
    if (clicked) ImGui::OpenPopup("options");
    ImGui::SetNextWindowPos(ImVec2(a.x, b.y+4), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, 260));
    bool changed = false;
    if (ImGui::BeginPopup("options")) {
        for (std::size_t i = 0; i < choices.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(choices[i].c_str(), selected == static_cast<int>(i))) {
                selected = static_cast<int>(i);
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    return changed;
}

void Separator(float space)
{
    ImGui::Dummy(ImVec2(0, space*0.5F));
    const ImVec2 a = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(a, ImVec2(a.x+ImGui::GetContentRegionAvail().x,a.y),
                                        color::Border, 1.0F);
    ImGui::Dummy(ImVec2(0, 1.0F+space*0.5F));
}
} // namespace m1core::ui
