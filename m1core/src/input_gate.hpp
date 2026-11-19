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
        focused_ = focused;
        if (!focused)
            Close();
    }

    void SetOtherOverlayActive(bool active) noexcept
    {
        otherOverlayActive_ = active;
        if (active)
            Close();
    }

    bool Toggle() noexcept
    {
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

    // ESC is a guaranteed exit: dispatch it before forwarding the event
    // to ImGui. Returns true only if the key was consumed by our overlay.
    bool Escape() noexcept
    {
        if (!open_)
            return false;
        Close();
        return true;
    }

    void Close() noexcept
    {
        open_ = false;
    }

    bool IsOpen() const noexcept { return open_; }

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
