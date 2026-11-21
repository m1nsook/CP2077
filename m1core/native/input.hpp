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

// The HWND may already be foreground before we subclass it: Windows does not
// guarantee a subsequent WM_SETFOCUS/WM_ACTIVATEAPP. Reconcile with the live
// desktop foreground on Present, just as the donated CET window bridge does.
[[nodiscard]] inline bool NeedsFocusReconciliation(bool cached, bool current) noexcept
{
    return cached != current;
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

// The engine cursor must be released whenever rendering, focus or UI state
// disallows capture; otherwise fullscreen Alt+Tab can strand gameplay input.
[[nodiscard]] inline bool NeedsEngineCursorSync(bool active, bool foreground,
                                                bool rendererReady,
                                                bool cursorHeld) noexcept
{
    return CanCaptureInput(active,foreground,rendererReady) != cursorHeld;
}

// Single-key bind selection follows the donated CET VKBindings approach:
// retain physical keyboard VKs, not just the function-key subset. Reject
// mouse buttons, OS modifiers and Windows-reserved virtual keys.
[[nodiscard]] inline bool IsBindableKeyboardKey(int key) noexcept
{
    if (key < VK_BACK || key >= VK_PACKET) return false;
    switch (key) {
    case VK_ESCAPE:
    case VK_SHIFT: case VK_CONTROL: case VK_MENU:
    case VK_LSHIFT: case VK_RSHIFT:
    case VK_LCONTROL: case VK_RCONTROL:
    case VK_LMENU: case VK_RMENU:
    case VK_LWIN: case VK_RWIN:
    case VK_SLEEP: case VK_PROCESSKEY:
        return false;
    default:
        return true;
    }
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
    void ReconcileWindowFocus() noexcept;
    // Preserve an open ImGui session during DX12 resize, fullscreen changes,
    // minimize and Alt+Tab. Only the render backend/cursor are suspended.
    void SuspendForGraphicsReset() noexcept;
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
    std::atomic_bool resetKeyEdgesOnWindowThread_{false};
    // When first-run commits on key-down, swallow its matching key-up
    // instead of reopening the overlay immediately on that release edge.
    int freshlyBoundKey_ = 0;
};
} // namespace m1core
