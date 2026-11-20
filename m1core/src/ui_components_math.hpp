#pragma once

#include <algorithm>
#include <cmath>

namespace m1core::ui
{
// Renderer-independent slider math. Snaps to each setting's supported step
// without ever writing outside its registered bounds.
inline double SnapSliderValue(double normalized, double min, double max, double step) noexcept
{
    if (!std::isfinite(min) || !std::isfinite(max) || max <= min)
        return min;
    const double ratio = std::isfinite(normalized) ?
        std::clamp(normalized, 0.0, 1.0) : 0.0;
    double value = min + ratio * (max - min);
    if (step > 0.0 && std::isfinite(step))
        value = min + std::round((value - min) / step) * step;
    return std::clamp(value, min, max);
}
} // namespace m1core::ui
