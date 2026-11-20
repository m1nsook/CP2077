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
bool g_hooksInstalled = false;
HWND g_gameWindow = nullptr;
int g_cursorShowAdjustments = 0;
constexpr int kMaxCursorShowAdjustments = 32;

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
        return false;
    }
    g_hooksInstalled = true;
    return true;
}

bool CursorDetoursAvailable()
{
    return g_hooksInstalled;
}

void SetCursorCaptured(bool active)
{
    if (!g_hooksInstalled || !g_originalClip) {
        g_capture.store(false, std::memory_order_release);
        return;
    }

    // Focus notifications can arrive on the window thread while Present is
    // running on the render thread: serialize pointer/clip transitions.
    std::lock_guard lock(g_clipMutex);
    const bool wasActive = g_capture.exchange(active, std::memory_order_acq_rel);
    if (wasActive == active)
        return;

    if (active) {
        RECT oldClip{};
        if (GetClipCursor(&oldClip)) {
            g_restoreClip = oldClip;
            g_hasRestoreClip = true;
        }
        // Call original APIs to prevent recursion in our user32 detours.
        g_originalClip(nullptr);
        ShowSystemPointerWhenNeeded();
    } else {
        // Never trap the pointer inside the game after switching monitors.
        const bool focused = g_gameWindow && GetForegroundWindow() == g_gameWindow;
        g_originalClip(focused && g_hasRestoreClip ? &g_restoreClip : nullptr);
        RestoreCursorVisibility();
    }
}

void EnsureSystemCursorVisible()
{
    if (!g_hooksInstalled)
        return;
    std::lock_guard lock(g_clipMutex);
    ShowSystemPointerWhenNeeded();
}

void RemoveCursorDetours()
{
    SetCursorCaptured(false);
    if (g_hooksInstalled && g_sdk && g_sdk->hooking) {
        // Restore BEFORE detaching, while both original trampolines exist.
        g_sdk->hooking->Detach(g_handle, g_clipTarget);
        g_sdk->hooking->Detach(g_handle, g_setPosTarget);
    }
    g_hooksInstalled = false;
    g_cursorShowAdjustments = 0;
    g_originalSetPos = nullptr;
    g_originalClip = nullptr;
    g_setPosTarget = nullptr;
    g_clipTarget = nullptr;
    g_sdk = nullptr;
    g_gameWindow = nullptr;
}
} // namespace m1core
