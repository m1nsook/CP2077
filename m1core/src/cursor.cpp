#include "cursor.hpp"

#include <RED4ext/CNamePool.hpp>
#include <RED4ext/GameEngine.hpp>

#include <Windows.h>
#include <atomic>
#include <cstdint>

namespace m1core {
namespace {
// m1core uses Cyberpunk's cursor service, rather than intercepting global
// SetCursorPos/ClipCursor or synthesizing raw mouse movement. The stable
// engine address hash is also used by CET, but this implementation neither
// includes nor copies CET's GPL source.
//
// Resolve the address gracefully: RED4ext::UniversalRelocFunc terminates
// the process if the symbol is unavailable. An optional development overlay
// must never do that to Cyberpunk.
constexpr std::uint32_t kForceCursorHash = 2130646213u;
using ForceCursorFn = void (*)(RED4ext::CBaseEngine::UnkD0*, RED4ext::CName, bool);
using ResolveFn = std::uintptr_t (*)(std::uint32_t);

std::atomic<ForceCursorFn> g_force{nullptr};
std::atomic<HWND> g_gameWindow{nullptr};
std::atomic<UINT> g_syncMessage{0};
std::atomic_bool g_requested{false};
std::atomic_bool g_updateQueued{false};
bool g_applied = false; // HWND thread only

bool IsWindowThread(HWND window)
{
    return window && GetWindowThreadProcessId(window, nullptr) == GetCurrentThreadId();
}

void ApplyEngineCursorOnWindowThread()
{
    const HWND window = g_gameWindow.load(std::memory_order_acquire);
    const auto function = g_force.load(std::memory_order_acquire);
    if (!window || !IsWindowThread(window) || !function)
        return;

    const bool wanted = g_requested.load(std::memory_order_acquire) &&
                        GetForegroundWindow() == window;
    if (wanted == g_applied)
        return;

    const auto* engine = RED4ext::CGameEngine::Get();
    if (!engine || !engine->unkD0)
        return;
    static const RED4ext::CName reason = RED4ext::CNamePool::Add("m1core");
    function(engine->unkD0, reason, wanted);
    g_applied = wanted;
}

void RequestWindowThreadUpdate()
{
    const HWND window = g_gameWindow.load(std::memory_order_acquire);
    const UINT message = g_syncMessage.load(std::memory_order_acquire);
    if (!window || !message)
        return;
    if (IsWindowThread(window)) {
        ApplyEngineCursorOnWindowThread();
        return;
    }
    if (g_updateQueued.exchange(true, std::memory_order_acq_rel))
        return;
    if (!PostMessageW(window, message, 0, 0))
        g_updateQueued.store(false, std::memory_order_release);
}
} // namespace

bool InstallCursorDetours(RED4ext::v1::PluginHandle, const RED4ext::v1::Sdk*,
                          HWND gameWindow)
{
    // Retain the public function name for one compatibility cycle; there are
    // NO SetCursorPos, ClipCursor or other process-wide detours installed.
    if (!gameWindow || !IsWindowThread(gameWindow))
        return false;

    HMODULE red4ext = GetModuleHandleW(L"RED4ext.dll");
    if (!red4ext)
        return false;
    const auto resolver = reinterpret_cast<ResolveFn>(
        GetProcAddress(red4ext, "RED4ext_ResolveAddress"));
    if (!resolver)
        return false;
    const auto address = resolver(kForceCursorHash);
    if (!address)
        return false;

    const UINT message = RegisterWindowMessageW(L"m1core.EngineCursorUpdate.v1");
    if (!message)
        return false;

    g_gameWindow.store(gameWindow, std::memory_order_release);
    g_syncMessage.store(message, std::memory_order_release);
    g_force.store(reinterpret_cast<ForceCursorFn>(address), std::memory_order_release);
    g_requested.store(false, std::memory_order_release);
    g_updateQueued.store(false, std::memory_order_release);
    g_applied = false;
    return true;
}

bool CursorDetoursAvailable()
{
    // Compatibility alias: this now reports ENGINE cursor availability.
    return g_force.load(std::memory_order_acquire) != nullptr;
}

void SetCursorCaptured(bool active, bool)
{
    if (!CursorDetoursAvailable()) return;
    const HWND window = g_gameWindow.load(std::memory_order_acquire);
    const bool desired = active && window && GetForegroundWindow() == window;
    if (g_requested.exchange(desired, std::memory_order_acq_rel) != desired)
        RequestWindowThreadUpdate();
    else if (IsWindowThread(window))
        ApplyEngineCursorOnWindowThread();
}

bool HandleCursorVisibilityMessage(UINT message)
{
    const UINT expected = g_syncMessage.load(std::memory_order_acquire);
    if (!expected || message != expected)
        return false;
    g_updateQueued.store(false, std::memory_order_release);
    ApplyEngineCursorOnWindowThread();
    return true;
}

void EnsureSystemCursorVisible()
{
    // Cyberpunk's ForceCursor owns show/hide and the ordinary Windows pointer
    // acceleration. Never tamper with the process/thread ShowCursor count.
}

void RestoreSystemCursorOnWindowThread()
{
    g_requested.store(false, std::memory_order_release);
    RequestWindowThreadUpdate();
}

void RemoveCursorDetours()
{
    // Synchronously release engine cursor on shutdown when possible; never
    // wait indefinitely for an unresponsive game window.
    g_requested.store(false, std::memory_order_release);
    const HWND window = g_gameWindow.load(std::memory_order_acquire);
    const UINT message = g_syncMessage.load(std::memory_order_acquire);
    if (window && message) {
        if (IsWindowThread(window)) {
            ApplyEngineCursorOnWindowThread();
        } else {
            DWORD_PTR ignored = 0;
            SendMessageTimeoutW(window, message, 0, 0,
                                SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &ignored);
        }
    }
    g_force.store(nullptr, std::memory_order_release);
    g_requested.store(false, std::memory_order_release);
    g_updateQueued.store(false, std::memory_order_release);
    g_gameWindow.store(nullptr, std::memory_order_release);
    g_syncMessage.store(0, std::memory_order_release);
    // g_applied is written only by the window thread and is reinitialized on install.
}
} // namespace m1core
