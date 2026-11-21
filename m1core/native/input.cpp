#include "input.hpp"

#include "render_bridge.hpp"
#include "renderer.hpp"
#include "state.hpp"

#include <RED4ext/RED4ext.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <string>

namespace m1core {
namespace {
constexpr std::uint32_t kGameForceCursor = 2130646213u;
using CursorFn = void (*)(RED4ext::CBaseEngine::UnkD0*,RED4ext::CName,bool);
constexpr RED4ext::CName kCursorReason("m1core_UI");
constexpr DWORD kBootstrapAfterMs = 12000;

bool IsKeyboardMessage(UINT message) noexcept
{
    return message>=WM_KEYFIRST && message<=WM_KEYLAST;
}
bool IsMouseMessage(UINT message) noexcept
{
    return message>=WM_MOUSEFIRST && message<=WM_MOUSELAST;
}
bool IsRawAltF4(LPARAM data) noexcept
{
    UINT size=0;
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(data),RID_INPUT,
                         nullptr,&size,sizeof(RAWINPUTHEADER)) != 0)
        return false;
    if (size==0 || size>4096) return false;
    alignas(RAWINPUT) BYTE buffer[4096]{};
    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(data),RID_INPUT,
                        buffer,&size,sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1))
        return false;
    const auto* input=reinterpret_cast<const RAWINPUT*>(buffer);
    return input->header.dwType==RIM_TYPEKEYBOARD &&
           input->data.keyboard.VKey==VK_F4 &&
           !(input->data.keyboard.Flags&RI_KEY_BREAK) &&
           (GetKeyState(VK_MENU)&0x8000)!=0;
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
    if (!IsWindow(hwnd) || !ResolveGameSymbol(kGameForceCursor))
        return false;

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
    mountedAt_=GetTickCount64();
    sawStartupKey_=false;
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
    if (IsWindow(window_) && engineCursorHeld_) {
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

void InputRouter::Close() noexcept
{
    visible_.store(false);
    setup_.store(false);
    recording_.store(false);
    RequestCursorSync();
}

void InputRouter::RequestCursorSync() noexcept
{
    if (window_ && IsWindow(window_))
        PostMessageW(window_,kCursorSyncMessage,0,0);
}

void InputRouter::ApplyEngineCursor(HWND hwnd) noexcept
{
    const bool desired=CanCaptureInput(visible_.load() || setup_.load(),
         focused_.load() && GetForegroundWindow()==hwnd,
         Renderer::Get().AcceptingInput());
    if (desired==engineCursorHeld_) return;
    const auto address=ResolveGameSymbol(kGameForceCursor);
    auto* engine=RED4ext::CGameEngine::Get();
    if (!address || !engine || !engine->unkD0) {
        // A broken/unavailable engine cursor must never trap keyboard input.
        visible_.store(false);
        setup_.store(false);
        recording_.store(false);
        return;
    }
    const auto cursor=reinterpret_cast<CursorFn>(address);
    cursor(engine->unkD0,kCursorReason,desired);
    engineCursorHeld_=desired;
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
        ApplyEngineCursor(hwnd);
        Renderer::Get().CancelInput();
        return next();
    }
    if (message==WM_SETFOCUS ||
        (message==WM_ACTIVATEAPP && wp)) {
        focused_.store(true);
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

    if (message==WM_INPUT && (visible_.load() || setup_.load()) &&
        IsRawAltF4(lp)) {
        Close();
        PostMessageW(hwnd,WM_CLOSE,0,0);
        return next();
    }

    // The player-chosen toggle must not accidentally trigger a vanilla
    // game action on keydown while we consume the matching keyup.
    if (message==WM_KEYDOWN || message==WM_SYSKEYDOWN) {
        if (!recording_.load() && hotkey_.load()!=0 &&
            static_cast<int>(wp)==hotkey_.load() &&
            (GetKeyState(VK_MENU)&0x8000)==0 &&
            (GetKeyState(VK_CONTROL)&0x8000)==0 &&
            (GetKeyState(VK_SHIFT)&0x8000)==0)
            return 1;
    }

    if (message==WM_KEYUP || message==WM_SYSKEYUP) {
        const int key=static_cast<int>(wp);
        if (recording_.load()) {
            if (key==VK_ESCAPE) {
                recording_.store(false);
                return 1;
            }
            if (IsValidShortcut(key) && SetMenuKey(key)) {
                recording_.store(false);
                setup_.store(false);
                visible_.store(false);
                RequestCursorSync();
            }
            return 1;
        }
        const int assigned=hotkey_.load();
        if (assigned!=0 && key==assigned &&
            (GetKeyState(VK_MENU)&0x8000)==0 &&
            (GetKeyState(VK_CONTROL)&0x8000)==0 &&
            (GetKeyState(VK_SHIFT)&0x8000)==0) {
            visible_.store(!visible_.load());
            RequestCursorSync();
            return 1;
        }
        // Bootstrap only AFTER game input has already been passed through.
        // Never steal the "Press any key to start" input during startup.
        if (!assigned && !setup_.load() &&
            GetTickCount64()-mountedAt_ >= kBootstrapAfterMs &&
            (key==VK_RETURN || key==VK_SPACE || key==VK_TAB)) {
            setup_.store(true);
            sawStartupKey_=true;
            RequestCursorSync();
            return next();
        }
    }

    if (CanCaptureInput(visible_.load() || setup_.load(),foreground,
                        Renderer::Get().AcceptingInput()) &&
        (IsKeyboardMessage(message) || IsMouseMessage(message) ||
         message==WM_INPUT)) {
        Renderer::Get().ForwardInput(hwnd,message,wp,lp);
        return 1;
    }
    return next();
}
} // namespace m1core
