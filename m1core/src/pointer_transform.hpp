#pragma once

#include <cmath>

namespace m1core {
// The physical OS pointer is reported in HWND client pixels. Dear ImGui uses
// swapchain/display coordinates, which may differ in exclusive fullscreen,
// display scaling, or resolution scaling. Convert positions, never raw deltas.
struct PointerPosition {
    float x = 0.0F;
    float y = 0.0F;
    bool valid = false;
};

inline PointerPosition MapClientPointer(long x, long y, long clientWidth,
                                        long clientHeight, float imguiWidth,
                                        float imguiHeight) noexcept
{
    if (clientWidth <= 0 || clientHeight <= 0 ||
        !std::isfinite(imguiWidth) || !std::isfinite(imguiHeight) ||
        imguiWidth <= 0.0F || imguiHeight <= 0.0F ||
        x < 0 || y < 0 || x >= clientWidth || y >= clientHeight)
        return {};
    const float mappedX = static_cast<float>(x) *
                          (imguiWidth / static_cast<float>(clientWidth));
    const float mappedY = static_cast<float>(y) *
                          (imguiHeight / static_cast<float>(clientHeight));
    return {mappedX, mappedY, true};
}
} // namespace m1core
