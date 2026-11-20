#pragma once

#include <algorithm>
#include <cmath>

namespace m1core::workspace
{
// Adapted from the user's NCREC WorkspaceLayout design: one fixed
// fullscreen canvas, independent regions, no draggable floating ImGui window.
// Layout geometry is pure C++ and can be tested without graphics.
struct Rect { float x{}, y{}, width{}, height{}; };
struct Layout {
    Rect screen{}, header{}, tabs{}, body{}, sidebar{}, content{}, footer{};
    bool showSidebar{};
};

inline Layout Calculate(float framebufferWidth, float framebufferHeight,
                        bool sidebarEnabled, float scale = 1.0f) noexcept
{
    Layout out{};
    if (!std::isfinite(framebufferWidth) || !std::isfinite(framebufferHeight) ||
        framebufferWidth <= 0 || framebufferHeight <= 0)
        return out;
    if (!std::isfinite(scale)) scale = 1.0f;
    scale = std::clamp(scale, 0.75f, 1.5f);
    const float w = framebufferWidth, h = framebufferHeight;
    const float header = std::min(h, 56.0f * scale);
    const float tabs = std::min(h - header, 44.0f * scale);
    const float footer = std::min(h - header - tabs, 56.0f * scale);
    const float middle = std::max(0.0f, h - header - tabs - footer);
    const float sidebar = sidebarEnabled && w >= 690.0f * scale
                        ? std::min(240.0f * scale, w * 0.32f) : 0.0f;
    out.showSidebar = sidebar > 0;
    out.screen = {0, 0, w, h};
    out.header = {0, 0, w, header};
    out.tabs = {0, header, w, tabs};
    out.body = {0, header + tabs, w, middle};
    out.sidebar = {0, out.body.y, sidebar, middle};
    out.content = {sidebar, out.body.y, w - sidebar, middle};
    out.footer = {0, h - footer, w, footer};
    return out;
}
} // namespace m1core::workspace
