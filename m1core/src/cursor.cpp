#include "cursor.hpp"

#include <RED4ext/RED4ext.hpp>

#include <Windows.h>
#include <atomic>
#include <mutex>

namespace m1core {
namespace {
using SetPosFn = BOOL(WINAPI*)(int, int);
using ClipFn = BOOL(WINAPI*)(const RECT*);

std::atomic_bool g_capture{false};
std::mutex g_clipMutex;
RECT g_restoreClip{};
bool g_hasRestoreClip = false;
SetPosFn g_originalSetPos = nullptr;
ClipFn g_originalClip = nullptr;
void* g_setPosTarget = nullptr;
void* g_clipTarget = nullptr;
RED4ext::v1::PluginHandle g_handle{};
const RED4ext::v1::Sdk* g_sdk = nullptr;
std::atomic_bool g_hooksInstalled{false};
HWND g_gameWindow = nullptr;
UINT g_cursorSyncMessage = 0;
std::atomic_bool g_visibilityUpdateQueued{false};
int g_cursorShowAdjustments = 0; // Modified only on the game HWND thread.
constexpr int kMaxCursorShowAdjustments = 32;

void ScheduleCursorVisibilityUpdate()
{
    if (!g_gameWindow || !g_cursorSyncMessage ||
        g_visibilityUpdateQueued.exchange(true, std::memory_order_acq_rel))
        return;
    if (!PostMessageW(g_gameWindow, g_cursorSyncMessage, 0, 0))
        g_visibilityUpdateQueued.store(false, std::memory_order_release);
}

// ShowCursor has a display counter, not a boolean. Balance only our own
// increments on close; never call it unconditionally every frame.
void ShowSystemPointerWhenNeeded()
{
    if (!g_capture.load(std::memory_order_acquire))
        return;
    CURSORINFO info{sizeof(info)};
    if (GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING))
        return;
    while (g_cursorShowAdjustments < kMaxCursorShowAdjustments)
    {
        ++g_cursorShowAdjustments;
        if (ShowCursor(TRUE) >= 0)
            break;
    }
}

void RestoreCursorVisibility()
{
    while (g_cursorShowAdjustments > 0)
    {
        ShowCursor(FALSE);
        --g_cursorShowAdjustments;
    }
}

BOOL WINAPI SetCursorPosHook(int x, int y)
{
    // Return success without recentering while m1core owns mouse input.
    if (g_capture.load(std::memory_order_acquire))
        return TRUE;
    return g_originalSetPos ? g_originalSetPos(x, y) : FALSE;
}

BOOL WINAPI ClipCursorHook(const RECT* requested)
{
    if (g_capture.load(std::memory_order_acquire)) {
        // Record the *latest* clip requested by the game, then keep mouse free.
        {
            std::lock_guard lock(g_clipMutex);
            if (requested) {
                g_restoreClip = *requested;
                g_hasRestoreClip = true;
            } else {
                g_hasRestoreClip = false;
            }
        }
        return g_originalClip ? g_originalClip(nullptr) : FALSE;
    }
    return g_originalClip ? g_originalClip(requested) : FALSE;
}
} // namespace

bool InstallCursorDetours(RED4ext::v1::PluginHandle plugin, const RED4ext::v1::Sdk* sdk, HWND gameWindow)
{
    if (g_hooksInstalled)
        return true;
    if (!sdk || !sdk->hooking)
        return false;
    auto* user32 = GetModuleHandleW(L"user32.dll");
    if (!user32)
        return false;
    g_setPosTarget = reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos"));
    g_clipTarget = reinterpret_cast<void*>(GetProcAddress(user32, "ClipCursor"));
    if (!g_setPosTarget || !g_clipTarget)
        return false;

    g_handle = plugin;
    g_sdk = sdk;
    g_gameWindow = gameWindow;
    g_cursorSyncMessage = RegisterWindowMessageW(L"m1core.cursor.visibility.v1");
    if (!g_cursorSyncMessage)
        return false;
    const bool setPosOk = sdk->hooking->Attach(plugin, g_setPosTarget,
        reinterpret_cast<void*>(&SetCursorPosHook),
        reinterpret_cast<void**>(&g_originalSetPos));
    const bool clipOk = setPosOk && sdk->hooking->Attach(plugin, g_clipTarget,
        reinterpret_cast<void*>(&ClipCursorHook),
        reinterpret_cast<void**>(&g_originalClip));
    if (!clipOk) {
        if (setPosOk)
            sdk->hooking->Detach(plugin, g_setPosTarget);
        g_originalSetPos = nullptr;
        g_originalClip = nullptr;
        g_gameWindow = nullptr;
        g_cursorSyncMessage = 0;
        return false;
    }
    g_hooksInstalled = true;
    return true;
}

bool CursorDetoursAvailable()
{
    return g_hooksInstalled;
}

void SetCursorCaptured(bool active, bool focusLost)
{
    if (!g_hooksInstalled || !g_originalClip) {
        g_capture.store(false, std::memory_order_release);
        return;
    }

    // Clip transitions can occur from the Present or HWND thread.
    {
        std::lock_guard lock(g_clipMutex);
        const bool wasActive = g_capture.exchange(active, std::memory_order_acq_rel);
        if (wasActive == active) {
            if (focusLost && !active)
                g_originalClip(nullptr);
            return;
        }

        if (active) {
            RECT oldClip{};
            if (GetClipCursor(&oldClip)) {
                g_restoreClip = oldClip;
                g_hasRestoreClip = true;
            }
            g_originalClip(nullptr);
        } else {
            // Never reapply a potentially stale centered clip captured before
            // the menu was opened. That can trap the Windows pointer or leave
            // the game/window unable to process normal close/Alt+Tab actions.
            // Release it now; while m1core is closed the game's own ClipCursor
            // calls pass directly through the detour and can recapture normally.
            g_originalClip(nullptr);
        }
    }
    // The ShowCursor counter is PER-THREAD. Never adjust it on the DX12
    // Present thread: hand visibility changes to the HWND owner instead.
    if (GetCurrentThreadId() == GetWindowThreadProcessId(g_gameWindow, nullptr)) {
        std::lock_guard lock(g_clipMutex);
        if (active) ShowSystemPointerWhenNeeded();
        else RestoreCursorVisibility();
    } else {
        ScheduleCursorVisibilityUpdate();
    }
}

bool HandleCursorVisibilityMessage(UINT message)
{
    if (!g_cursorSyncMessage || message != g_cursorSyncMessage)
        return false;
    g_visibilityUpdateQueued.store(false, std::memory_order_release);
    std::lock_guard lock(g_clipMutex);
    if (g_capture.load(std::memory_order_acquire))
        ShowSystemPointerWhenNeeded();
    else
        RestoreCursorVisibility();
    return true;
}

void RestoreSystemCursorOnWindowThread()
{
    std::lock_guard lock(g_clipMutex);
    RestoreCursorVisibility();
}

void EnsureSystemCursorVisible()
{
    if (!g_hooksInstalled || !g_capture.load(std::memory_order_acquire))
        return;
    CURSORINFO info{sizeof(info)};
    if (GetCursorInfo(&info) && (info.flags & CURSOR_SHOWING))
        return;
    if (GetCurrentThreadId() == GetWindowThreadProcessId(g_gameWindow, nullptr)) {
        std::lock_guard lock(g_clipMutex);
        ShowSystemPointerWhenNeeded();
    } else {
        ScheduleCursorVisibilityUpdate();
    }
}

void RemoveCursorDetours()
{
    SetCursorCaptured(false);
    if (g_gameWindow && g_cursorSyncMessage &&
        GetCurrentThreadId() != GetWindowThreadProcessId(g_gameWindow, nullptr))
    {
        // Flush a pending visibility restoration on the HWND thread before
        // detaching, without ever waiting indefinitely for a hung window.
        DWORD_PTR ignored = 0;
        SendMessageTimeoutW(g_gameWindow, g_cursorSyncMessage, 0, 0,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 250, &ignored);
    }
    else if (g_gameWindow)
        RestoreSystemCursorOnWindowThread();
    if (g_hooksInstalled && g_sdk && g_sdk->hooking) {
        // Restore BEFORE detaching, while both original trampolines exist.
        g_sdk->hooking->Detach(g_handle, g_clipTarget);
        g_sdk->hooking->Detach(g_handle, g_setPosTarget);
    }
    g_hooksInstalled = false;
    g_visibilityUpdateQueued.store(false, std::memory_order_release);
    g_cursorSyncMessage = 0;
    // All ShowCursor adjustments were restored on the HWND thread above.
    g_cursorShowAdjustments = 0;
    g_originalSetPos = nullptr;
    g_originalClip = nullptr;
    g_setPosTarget = nullptr;
    g_clipTarget = nullptr;
    g_sdk = nullptr;
    g_gameWindow = nullptr;
}
} // namespace m1core
