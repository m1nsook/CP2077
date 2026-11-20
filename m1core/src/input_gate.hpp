#pragma once

namespace m1core
{
// Pure state machine for future overlay/input integration.
//
// The renderer and Win32 message handler must use this as the sole source
// of truth for whether they may take keyboard/mouse input away from the game.
// In particular, the overlay *never* captures input if rendering is not ready,
// if the game window loses focus, or if another overlay owns input.
class InputGate final
{
public:
    void SetRendererReady(bool ready) noexcept
    {
        rendererReady_ = ready;
        if (!ready)
            Close();
    }

    void SetGameFocused(bool focused) noexcept
    {
        // Alt-tab or clicking another monitor must release game input without
        // dismissing the settings window. The player-selected key closes it.
        focused_ = focused;
    }

    void SetOtherOverlayActive(bool active) noexcept
    {
        otherOverlayActive_ = active;
        if (active)
            Close();
    }

    bool Toggle() noexcept
    {
        // Ignore toggles while using a different desktop window.
        if (!focused_)
            return false;
        if (open_)
        {
            Close();
            return false;
        }
        if (!rendererReady_ || !focused_ || otherOverlayActive_)
            return false;
        open_ = true;
        return true;
    }

    void Close() noexcept
    {
        open_ = false;
    }

    bool IsOpen() const noexcept { return open_; }
    bool IsGameFocused() const noexcept { return focused_; }

    bool MayCaptureInput() const noexcept
    {
        return open_ && rendererReady_ && focused_ && !otherOverlayActive_;
    }

private:
    bool rendererReady_ = false;
    bool focused_ = false;
    bool otherOverlayActive_ = false;
    bool open_ = false;
};
} // namespace m1core
