#pragma once

#include <Windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace m1core {

constexpr UINT kCursorSyncMessage = WM_APP + 0x4B3;

[[nodiscard]] inline bool MustPassThroughWindows(UINT message,WPARAM wparam,
                                                   LPARAM lparam) noexcept
{
    if (message == WM_CLOSE || message == WM_DESTROY ||
        message == WM_NCDESTROY || message == WM_QUIT)
        return true;
    if (message == WM_SYSCOMMAND && (wparam & 0xFFF0u) == SC_CLOSE)
        return true;
    // Even an F4 keybind may never override Windows' Alt+F4 command.
    return message == WM_SYSKEYDOWN && wparam == VK_F4 &&
           (static_cast<std::uintptr_t>(lparam) & (1ull << 29)) != 0;
}

[[nodiscard]] inline bool OwnsInput(bool visible, bool foreground) noexcept
{
    return visible && foreground;
}

// Rendering can be requested before the DX12/ImGui backend is usable,
// but game input must remain untouched until a frame actually submitted.
[[nodiscard]] inline bool CanCaptureInput(bool visible, bool foreground,
                                          bool rendererReady) noexcept
{
    return OwnsInput(visible, foreground) && rendererReady;
}

// First-run keyboard binding does not require ForceCursor to be ready.
// Only the full mouse/ImGui interaction needs native cursor ownership.
[[nodiscard]] inline bool CanRecordKey(bool setup, bool foreground,
                                      bool rendererReady) noexcept
{
    return CanCaptureInput(setup, foreground, rendererReady);
}

// General UI input is allowed only when Cyberpunk's own cursor is held.
// Keyboard-only first-run binding is handled separately above.
[[nodiscard]] inline bool CanRouteImGuiInput(bool active, bool foreground,
                                            bool rendererReady,
                                            bool cursorHeld) noexcept
{
    return CanCaptureInput(active, foreground, rendererReady) && cursorHeld;
}

enum class KeyEdge { None, Press, Release };
[[nodiscard]] inline KeyEdge TrackKeyEdge(std::array<bool,256>& keys,
                                          unsigned key,bool pressed) noexcept
{
    if (key >= keys.size() || keys[key] == pressed) return KeyEdge::None;
    keys[key] = pressed;
    return pressed ? KeyEdge::Press : KeyEdge::Release;
}

class InputRouter {
public:
    static InputRouter& Get() noexcept;
    [[nodiscard]] bool Install(HWND gameWindow) noexcept;
    void Uninstall() noexcept;
    [[nodiscard]] bool Ready() const noexcept { return window_ != nullptr; }
    [[nodiscard]] bool ShouldDraw() const noexcept;
    [[nodiscard]] bool KeyAssigned() const noexcept { return hotkey_.load() != 0; }
    [[nodiscard]] std::string KeyDescription() const;
    [[nodiscard]] bool Recording() const noexcept { return recording_.load(); }
    [[nodiscard]] bool NeedsPopupDismiss() const noexcept {
        return popupDismiss_.load(std::memory_order_acquire);
    }
    void AcknowledgePopupDismiss() noexcept {
        popupDismiss_.store(false, std::memory_order_release);
    }
    [[nodiscard]] bool CursorHeld() const noexcept { return engineCursorHeld_.load(); }
    // Called only on the game render callback while a native UI is wanted.
    void RetryCursorSync() noexcept;
    void BeginKeyCapture() noexcept;
    // Called after ImGui/DX12 backends initialize, not from a startup key.
    void ArmFirstRun() noexcept;
    void Close() noexcept;
    void RequestCursorSync() noexcept;
private:
    static LRESULT CALLBACK WindowProc(HWND window,UINT message,
                                       WPARAM wparam,LPARAM lparam);
    LRESULT Dispatch(HWND window,UINT message,WPARAM wparam,LPARAM lparam) noexcept;
    void ApplyEngineCursor(HWND window) noexcept;
    bool SetMenuKey(int key) noexcept;
    // Process raw and translated Win32 keyboard messages exactly once.
    [[nodiscard]] bool HandleKeyTransition(int key,bool pressed) noexcept;
    void ReloadKeyFromDisk() noexcept;
    [[nodiscard]] bool IsValidShortcut(int key) const noexcept;

    HWND window_ = nullptr;
    WNDPROC previous_ = nullptr;
    std::atomic_bool focused_{false};
    std::atomic_bool visible_{false};
    std::atomic_bool setup_{false};
    std::atomic_bool recording_{false};
    std::atomic<int> hotkey_{0};
    std::atomic_bool engineCursorHeld_{false};
    std::atomic<std::uint64_t> lastCursorRetry_{0};
    bool loggedCursorUnavailable_ = false;
    std::array<bool,256> keysDown_{};
    std::atomic_bool firstRunArmed_{false};
    std::atomic_bool popupDismiss_{false};
};
} // namespace m1core
