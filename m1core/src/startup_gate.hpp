#pragma once

#include <atomic>

namespace m1core
{
// Startup safety gate: CET-style rendering/window integration may install
// early but must remain completely transparent to title-screen input.
// Capturing input, showing the menu and installing global cursor detours
// require a real PlayerPuppet attach after gameplay persistence hydration.
class StartupGate final
{
public:
    void Reset() noexcept { ready_.store(false, std::memory_order_release); }

    // true only for the first attach in this launch.
    bool MarkPlayerAttached() noexcept
    {
        return !ready_.exchange(true, std::memory_order_acq_rel);
    }

    bool GameplayReady() const noexcept
    {
        return ready_.load(std::memory_order_acquire);
    }

private:
    std::atomic_bool ready_{false};
};
} // namespace m1core
