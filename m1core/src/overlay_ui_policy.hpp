#pragma once

namespace m1core {
inline bool ShouldDrawOverlayCursor(bool capturing, bool gameFocused) noexcept
{
    return capturing && gameFocused;
}
inline bool NeedsCompactSetup(int userMenuKey) noexcept
{
    return userMenuKey <= 0;
}
} // namespace m1core
