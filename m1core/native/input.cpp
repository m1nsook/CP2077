#include "input.hpp"

#include "render_bridge.hpp"
#include "renderer.hpp"
#include "state.hpp"

#include <RED4ext/RED4ext.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <optional>
#include <string>

namespace m1core {
namespace {
constexpr std::uint32_t kGameForceCursor = 2130646213u;
using CursorFn = void (*)(RED4ext::CBaseEngine::UnkD0*,RED4ext::CName,bool);
constexpr RED4ext::CName kCursorReason("m1core_UI");

bool IsKeyboardMessage(UINT message) noexcept
{
    return message>=WM_KEYFIRST && message<=WM_KEYLAST;
}
bool IsMouseMessage(UINT message) noexcept
{
    return message>=WM_MOUSEFIRST && message<=WM_MOUSELAST;
}
std::optional<RAWKEYBOARD> ReadRawKeyboard(LPARAM data) noexcept
{
    UINT size = 0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(data), RID_INPUT,
                        nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 ||
        size < sizeof(RAWINPUTHEADER) || size > 4096) return std::nullopt;
    alignas(RAWINPUT) BYTE buffer[4096]{};
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(data), RID_INPUT,
                        buffer, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        return std::nullopt;
    const auto* raw = reinterpret_cast<const RAWINPUT*>(buffer);
    if (raw->header.dwType != RIM_TYPEKEYBOARD) return std::nullopt;
    return raw->data.keyboard;
}

std::string KeyName(int key)
{
    if (key==0) return "Not assigned";
    wchar_t name[128]{};
    UINT mapped=MapVirtualKeyW(static_cast<UINT>(key),MAPVK_VK_TO_VSC);
    if (key==VK_LEFT || key==VK_RIGHT || key==VK_UP || key==VK_DOWN ||
        key==VK_INSERT || key==VK_DELETE || key==VK_HOME || key==VK_END)
        mapped|=0xE000;
    const LONG scan=static_cast<LONG>(mapped<<16);
    if (GetKeyNameTextW(scan,name,128)>0) {
        char utf8[256]{};
        if (WideCharToMultiByte(CP_UTF8,0,name,-1,utf8,256,nullptr,nullptr)>0)
            return utf8;
    }
    char unknown[24]{};
    std::snprintf(unknown,sizeof(unknown),"Key %02X",key);
    return unknown;
}
}

InputRouter& InputRouter::Get() noexcept
{
    static InputRouter input;
    return input;
}

void InputRouter::ReloadKeyFromDisk() noexcept
{
    int key=0;
    try {
        const auto data=nlohmann::json::parse(Store().Snapshot());
        if (data.contains("overlay") && data["overlay"].is_object() &&
            data["overlay"].contains("hotkey") &&
            data["overlay"]["hotkey"].is_number_integer()) {
            const int saved=data["overlay"]["hotkey"].get<int>();
            if (IsValidShortcut(saved)) key=saved;
        }
    } catch (...) {}
    hotkey_.store(key);
}

bool InputRouter::Install(HWND hwnd) noexcept
{
    if (window_==hwnd && previous_) return true;
    if (window_) Uninstall();
    // A missing/late ForceCursor symbol must never prevent mounting
    // the keyboard-only first-run binder or native ImGui renderer.
    // General mouse input still requires actual cursor ownership.
    if (!IsWindow(hwnd)) return false;

    SetLastError(0);
    const LONG_PTR prior=SetWindowLongPtrW(hwnd,GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(&WindowProc));
    if (!prior && GetLastError()!=0) return false;
    previous_=reinterpret_cast<WNDPROC>(prior);
    if (!previous_) {
        // Never leave a subclass on the game without a callable original.
        SetWindowLongPtrW(hwnd,GWLP_WNDPROC,prior);
        return false;
    }
    window_=hwnd;
    firstRunArmed_.store(false);
    popupDismiss_.store(false);
    engineCursorHeld_.store(false);
    lastCursorRetry_.store(0);
    loggedCursorUnavailable_ = false;
    keysDown_.fill(false);
    focused_.store(GetForegroundWindow()==hwnd);
    ReloadKeyFromDisk();
    visible_.store(false);
    setup_.store(false);
    recording_.store(false);
    return true;
}

void InputRouter::Uninstall() noexcept
{
    if (!window_) return;
    visible_.store(false);
    setup_.store(false);
    recording_.store(false);
    focused_.store(false);
    keysDown_.fill(false);
    if (IsWindow(window_) && CursorHeld()) {
        const DWORD ownerThread = GetWindowThreadProcessId(window_,nullptr);
        if (ownerThread == GetCurrentThreadId()) {
            ApplyEngineCursor(window_);
        } else {
            // Don't invoke a game cursor API from the renderer thread.
            // Deliver release on the actual game HWND thread, bounded so
            // shutdown can never hang if the game window is unresponsive.
            DWORD_PTR result{};
            SendMessageTimeoutW(window_,kCursorSyncMessage,0,0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK,250,&result);
        }
    }
    if (IsWindow(window_) && previous_ &&
        reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window_,GWLP_WNDPROC)) ==
        &WindowProc)
        SetWindowLongPtrW(window_,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(previous_));
    previous_=nullptr;
    window_=nullptr;
}

bool InputRouter::ShouldDraw() const noexcept
{
    return window_ && OwnsInput(visible_.load() || setup_.load(),
                              focused_.load() && GetForegroundWindow()==window_);
}

bool InputRouter::IsValidShortcut(int key) const noexcept
{
    return (key>=VK_F1 && key<=VK_F24) ||
           (key>='0' && key<='9') || (key>='A' && key<='Z') ||
           key==VK_INSERT || key==VK_HOME || key==VK_END ||
           key==VK_PAUSE || key==VK_OEM_3;
}

bool InputRouter::SetMenuKey(int key) noexcept
{
    if (!IsValidShortcut(key)) return false;
    if (!Store().Update([key](nlohmann::json& state) {
            state["overlay"]=nlohmann::json::object({{"hotkey",key}});
        })) return false;
    if (setup_.load()) popupDismiss_.store(true,std::memory_order_release);
    hotkey_.store(key);
    return true;
}

std::string InputRouter::KeyDescription() const
{
    return KeyName(hotkey_.load());
}

void InputRouter::BeginKeyCapture() noexcept
{
    recording_.store(true);
    if (!visible_.load()) setup_.store(true);
    RequestCursorSync();
}

bool InputRouter::HandleKeyTransition(int key,bool down) noexcept
{
    if (key < 0 || key >= 256) return false;
    const KeyEdge edge = TrackKeyEdge(keysDown_,static_cast<unsigned>(key),down);
    // Consume the assigned toggle down/up even on duplicate message paths.
    const bool plain = (GetKeyState(VK_MENU)&0x8000)==0 &&
                       (GetKeyState(VK_CONTROL)&0x8000)==0 &&
                       (GetKeyState(VK_SHIFT)&0x8000)==0;
    if (recording_.load()) {
        // Do not make first-run KEY binding depend on mouse cursor startup.
        // Only accept input after an ImGui frame is submitted and this HWND
        // is foreground; the game launch prompt is never captured early.
        if (!CanRecordKey(visible_.load() || setup_.load(),
            focused_.load() && GetForegroundWindow()==window_,
            Renderer::Get().AcceptingInput()))
            return false;
        if (edge==KeyEdge::Release) {
            if (key==VK_ESCAPE) {
                recording_.store(false);
            } else if (plain && IsValidShortcut(key) && SetMenuKey(key)) {
                RenderBridge::Get().LogInfo("m1core: overlay hotkey bound successfully");
                recording_.store(false);
                setup_.store(false);
                visible_.store(false);
                RequestCursorSync();
            }
        }
        return true;
    }
    if (hotkey_.load()!=0 && key==hotkey_.load() && plain) {
        if (edge==KeyEdge::Release) {
            const bool nowOpen = !visible_.load();
            visible_.store(nowOpen);
            if (!nowOpen) popupDismiss_.store(true,std::memory_order_release);
            RequestCursorSync();
        }
        return true;
    }
    return false;
}

void InputRouter::ArmFirstRun() noexcept
{
    if (KeyAssigned() || firstRunArmed_.exchange(true)) return;
    // Match CET's post-initialization first-run behavior without ever
    // hijacking the game's Enter/Space/Tab startup keys.
    setup_.store(true);
    recording_.store(true); // Keyboard works immediately; no mouse click required.
    RenderBridge::Get().LogInfo("m1core: first-run setup armed for direct key capture");
    RequestCursorSync();
}

void InputRouter::Close() noexcept
{
    if (!KeyAssigned()) firstRunArmed_.store(false);
    if (visible_.exchange(false) || setup_.exchange(false))
        popupDismiss_.store(true,std::memory_order_release);
    recording_.store(false);
    RequestCursorSync();
}

void InputRouter::RequestCursorSync() noexcept
{
    if (window_ && IsWindow(window_))
        PostMessageW(window_,kCursorSyncMessage,0,0);
}

void InputRouter::RetryCursorSync() noexcept
{
    if (!ShouldDraw() || CursorHeld() ||
        !Renderer::Get().AcceptingInput()) return;
    const auto now = GetTickCount64();
    auto previous = lastCursorRetry_.load();
    if (now - previous < 350 ||
        !lastCursorRetry_.compare_exchange_strong(previous,now))
        return;
    RequestCursorSync();
}

void InputRouter::ApplyEngineCursor(HWND hwnd) noexcept
{
    const bool desired=CanCaptureInput(visible_.load() || setup_.load(),
         focused_.load() && GetForegroundWindow()==hwnd,
         Renderer::Get().AcceptingInput());
    if (desired==CursorHeld()) return;
    const auto address=ResolveGameSymbol(kGameForceCursor);
    auto* engine=RED4ext::CGameEngine::Get();
    if (!address || !engine || !engine->unkD0) {
        // During early initialization the game engine may not yet have a
        // cursor system. Do not permanently dismiss first-run setup:
        // the next valid Present will retry without consuming game input.
        if (!loggedCursorUnavailable_) {
            RenderBridge::Get().LogWarn(
                "m1core: game cursor not ready; waiting without capturing input");
            loggedCursorUnavailable_ = true;
        }
        return;
    }
    const auto cursor=reinterpret_cast<CursorFn>(address);
    cursor(engine->unkD0,kCursorReason,desired);
    engineCursorHeld_.store(desired);
    loggedCursorUnavailable_ = false;
    RenderBridge::Get().LogInfo(desired ?
        "m1core: native cursor acquired; input capture enabled" :
        "m1core: native cursor released");
}

LRESULT CALLBACK InputRouter::WindowProc(HWND hwnd,UINT message,
                                          WPARAM wp,LPARAM lp)
{
    return Get().Dispatch(hwnd,message,wp,lp);
}

LRESULT InputRouter::Dispatch(HWND hwnd,UINT message,
                               WPARAM wp,LPARAM lp) noexcept
{
    const WNDPROC previous=previous_;
    if (!previous) return DefWindowProcW(hwnd,message,wp,lp);
    const auto next=[&] { return CallWindowProcW(previous,hwnd,message,wp,lp); };
    if (hwnd!=window_) return next();

    if (message==kCursorSyncMessage) {
        ApplyEngineCursor(hwnd);
        return 0;
    }
    if (message==WM_KILLFOCUS ||
        (message==WM_ACTIVATEAPP && !wp)) {
        focused_.store(false);
        recording_.store(false); // Never bind keys typed after returning from Alt+Tab.
        keysDown_.fill(false);
        ApplyEngineCursor(hwnd);
        Renderer::Get().CancelInput();
        return next();
    }
    if (message==WM_SETFOCUS ||
        (message==WM_ACTIVATEAPP && wp)) {
        focused_.store(true);
        keysDown_.fill(false);
        if (setup_.load() && !KeyAssigned())
            recording_.store(true); // Resume first-run capture after Alt+Tab.
        Renderer::Get().ResumeInput();
        RequestCursorSync();
        return next();
    }
    if (MustPassThroughWindows(message,wp,lp)) {
        if (message==WM_CLOSE || message==WM_DESTROY || message==WM_NCDESTROY) {
            visible_.store(false);
            setup_.store(false);
            focused_.store(false);
            ApplyEngineCursor(hwnd);
        }
        return next();
    }

    const bool foreground=focused_.load() && GetForegroundWindow()==hwnd;
    if (!foreground) return next();

    // CET likewise detects gameplay hotkeys via WM_INPUT. Modern games
    // may send both raw and translated key messages; TrackKeyEdge
    // guarantees one activation per physical release.
    if (message==WM_INPUT) {
        if (const auto raw=ReadRawKeyboard(lp)) {
            const bool down=(raw->Flags & RI_KEY_BREAK)==0;
            if (raw->VKey==VK_F4 && down &&
                (GetKeyState(VK_MENU)&0x8000)!=0 &&
                (visible_.load() || setup_.load())) {
                Close();
                PostMessageW(hwnd,WM_CLOSE,0,0);
                return next();
            }
            if (HandleKeyTransition(static_cast<int>(raw->VKey),down)) {
                // Cleanup for consumed RIM_INPUT messages is mandatory.
                return DefWindowProcW(hwnd,message,wp,lp);
            }
        }
    }
    if (message==WM_KEYDOWN || message==WM_SYSKEYDOWN ||
        message==WM_KEYUP || message==WM_SYSKEYUP) {
        const bool down=message==WM_KEYDOWN || message==WM_SYSKEYDOWN;
        if (HandleKeyTransition(static_cast<int>(wp),down))
            return 1;
    }

    if (CanRouteImGuiInput(visible_.load() || setup_.load(),foreground,
                           Renderer::Get().AcceptingInput(),CursorHeld()) &&
        (IsKeyboardMessage(message) || IsMouseMessage(message) ||
         message==WM_INPUT)) {
        Renderer::Get().ForwardInput(hwnd,message,wp,lp);
        return 1;
    }
    return next();
}
} // namespace m1core
