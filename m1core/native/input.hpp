#pragma once

#include <Windows.h>
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
    void BeginKeyCapture() noexcept;
    void Close() noexcept;
    void RequestCursorSync() noexcept;
private:
    static LRESULT CALLBACK WindowProc(HWND window,UINT message,
                                       WPARAM wparam,LPARAM lparam);
    LRESULT Dispatch(HWND window,UINT message,WPARAM wparam,LPARAM lparam) noexcept;
    void ApplyEngineCursor(HWND window) noexcept;
    bool SetMenuKey(int key) noexcept;
    void ReloadKeyFromDisk() noexcept;
    [[nodiscard]] bool IsValidShortcut(int key) const noexcept;

    HWND window_ = nullptr;
    WNDPROC previous_ = nullptr;
    std::atomic_bool focused_{false};
    std::atomic_bool visible_{false};
    std::atomic_bool setup_{false};
    std::atomic_bool recording_{false};
    std::atomic<int> hotkey_{0};
    bool engineCursorHeld_ = false;
    std::uint64_t mountedAt_ = 0;
    bool sawStartupKey_ = false;
};
} // namespace m1core
