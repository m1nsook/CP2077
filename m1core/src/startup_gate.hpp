#pragma once

#include <atomic>

namespace m1core
{
// Startup safety gate: never install window, renderer or cursor hooks during
// the logos / "Press to Continue" title sequence. Native settings APIs still
// function, but the overlay activates only after V attaches to a loaded game.
class StartupGate final
{
public:
    void Reset() noexcept { ready_.store(false, std::memory_order_release); }

    // true only for the first attach in this launch.
    bool MarkPlayerAttached() noexcept
    {
        return !ready_.exchange(true, std::memory_order_acq_rel);
    }

    bool MayInstallOverlayHooks() const noexcept
    {
        return ready_.load(std::memory_order_acquire);
    }

private:
    std::atomic_bool ready_{false};
};
} // namespace m1core
