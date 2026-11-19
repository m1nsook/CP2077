#include "raw_input.hpp"

#include <imgui.h>
#include <backends/imgui_impl_win32.h>

// Dear ImGui leaves this declaration to the application's Win32 source.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#include <algorithm>
#include <array>
#include <cmath>

namespace m1core {
namespace {
constexpr std::size_t kRawLimit = sizeof(RAWINPUT) + 64;

bool IsKeyMessage(UINT message)
{
    return message == WM_KEYDOWN || message == WM_KEYUP ||
           message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
}

bool IsMouseWheelMessage(UINT message)
{
    return message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL;
}

void FeedRawButtons(ImGuiIO& io, USHORT flags)
{
    struct Button { USHORT down; USHORT up; int index; };
    constexpr Button buttons[] = {
        {RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, 0},
        {RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, 1},
        {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, 2},
        {RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, 3},
        {RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP, 4}
    };
    for (const auto& b : buttons) {
        if (flags & b.down) io.AddMouseButtonEvent(b.index, true);
        if (flags & b.up) io.AddMouseButtonEvent(b.index, false);
    }
}

void FeedRawKey(HWND window, const RawSample& sample)
{
    const bool released = (sample.keyFlags & RI_KEY_BREAK) != 0;
    if (sample.vkey == 0 || sample.vkey == 255)
        return;
    LPARAM data = static_cast<LPARAM>(sample.makeCode & 0xFF) << 16;
    if (sample.keyFlags & RI_KEY_E0) data |= static_cast<LPARAM>(1) << 24;
    if (released) data |= static_cast<LPARAM>(1) << 31;
    ImGui_ImplWin32_WndProcHandler(window, released ? WM_KEYUP : WM_KEYDOWN,
                                   sample.vkey, data);

    // RAWINPUT with RIDEV_NOLEGACY does not generate WM_CHAR. Translate only
    // printable key-downs. Flag 4 makes ToUnicodeEx avoid mutating dead-key
    // state on Windows 10+.
    if (released || sample.vkey < VK_SPACE || sample.vkey >= VK_F1 ||
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
        (GetAsyncKeyState(VK_MENU) & 0x8000))
        return;
    BYTE state[256]{};
    GetKeyboardState(state);
    const auto pressed = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    state[VK_SHIFT] = pressed(VK_SHIFT) ? 0x80 : 0x00;
    state[VK_LSHIFT] = pressed(VK_LSHIFT) ? 0x80 : 0x00;
    state[VK_RSHIFT] = pressed(VK_RSHIFT) ? 0x80 : 0x00;
    state[VK_CAPITAL] = (GetKeyState(VK_CAPITAL) & 1) ? 1 : 0;
    wchar_t characters[8]{};
    const auto thread = GetWindowThreadProcessId(window, nullptr);
    const int count = ToUnicodeEx(sample.vkey, sample.makeCode, state, characters,
                                 8, 4, GetKeyboardLayout(thread));
    if (count > 0) {
        for (int i = 0; i < count; ++i)
            ImGui::GetIO().AddInputCharacterUTF16(characters[i]);
    }
}
} // namespace

bool DecodeRawInput(LPARAM rawHandle, RawSample& out)
{
    UINT size = 0;
    const HRAWINPUT handle = reinterpret_cast<HRAWINPUT>(rawHandle);
    if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 ||
        size < sizeof(RAWINPUTHEADER) || size > kRawLimit)
        return false;

    alignas(RAWINPUT) std::array<BYTE, kRawLimit> buffer{};
    const UINT actual = GetRawInputData(handle, RID_INPUT, buffer.data(), &size,
                                        sizeof(RAWINPUTHEADER));
    if (actual == static_cast<UINT>(-1) || actual < sizeof(RAWINPUTHEADER))
        return false;

    const auto& raw = *reinterpret_cast<const RAWINPUT*>(buffer.data());
    out.type = raw.header.dwType;
    if (out.type == RIM_TYPEMOUSE) {
        out.dx = raw.data.mouse.lLastX;
        out.dy = raw.data.mouse.lLastY;
        out.mouseFlags = raw.data.mouse.usFlags;
        out.buttonFlags = raw.data.mouse.usButtonFlags;
        out.buttonData = raw.data.mouse.usButtonData;
        return true;
    }
    if (out.type == RIM_TYPEKEYBOARD) {
        out.vkey = raw.data.keyboard.VKey;
        out.keyFlags = raw.data.keyboard.Flags;
        out.makeCode = raw.data.keyboard.MakeCode;
        return true;
    }
    return false;
}

bool ForwardToGame(const RawSample& sample)
{
    if (sample.type == RIM_TYPEKEYBOARD)
        return (sample.keyFlags & RI_KEY_BREAK) != 0;
    if (sample.type == RIM_TYPEMOUSE) {
        constexpr USHORT presses = RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_DOWN |
                                   RI_MOUSE_MIDDLE_BUTTON_DOWN | RI_MOUSE_BUTTON_4_DOWN |
                                   RI_MOUSE_BUTTON_5_DOWN;
        constexpr USHORT releases = RI_MOUSE_LEFT_BUTTON_UP | RI_MOUSE_RIGHT_BUTTON_UP |
                                    RI_MOUSE_MIDDLE_BUTTON_UP | RI_MOUSE_BUTTON_4_UP |
                                    RI_MOUSE_BUTTON_5_UP;
        return (sample.buttonFlags & presses) == 0 &&
               (sample.buttonFlags & releases) != 0;
    }
    return true;
}

void RawInputBridge::Reset() noexcept
{
    virtualActive_ = false;
    rawKeyboardObserved_ = false;
    positionReady_ = false;
    mouseX_ = 0.0F;
    mouseY_ = 0.0F;
}

void RawInputBridge::Feed(HWND window, const std::vector<MSG>& legacy,
                          const std::vector<RawSample>& raw)
{
    ImGuiIO& io = ImGui::GetIO();
    // With queued platform + raw events in one frame, consume all of them
    // rather than trickling a physical recenter over the virtual pointer.
    io.ConfigInputTrickleEventQueue = false;

    bool legacyKeyboard = false;
    bool legacyWheel = false;
    for (const MSG& msg : legacy) {
        legacyKeyboard |= IsKeyMessage(msg.message) || msg.message == WM_CHAR;
        legacyWheel |= IsMouseWheelMessage(msg.message);
        ImGui_ImplWin32_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    }

    if (!positionReady_) {
        mouseX_ = io.DisplaySize.x * 0.5F;
        mouseY_ = io.DisplaySize.y * 0.5F;
        positionReady_ = true;
    }

    for (const RawSample& sample : raw) {
        if (sample.type == RIM_TYPEMOUSE &&
            (sample.mouseFlags & MOUSE_MOVE_ABSOLUTE) == 0 &&
            (sample.dx || sample.dy)) {
            virtualActive_ = true;
            mouseX_ += static_cast<float>(sample.dx);
            mouseY_ += static_cast<float>(sample.dy);
        }
    }
    if (virtualActive_) {
        mouseX_ = std::clamp(mouseX_, 0.0F, (std::max)(0.0F, io.DisplaySize.x - 1.0F));
        mouseY_ = std::clamp(mouseY_, 0.0F, (std::max)(0.0F, io.DisplaySize.y - 1.0F));
        io.AddMousePosEvent(mouseX_, mouseY_);
    }

    for (const RawSample& sample : raw) {
        if (sample.type == RIM_TYPEMOUSE) {
            FeedRawButtons(io, sample.buttonFlags);
            if ((sample.buttonFlags & RI_MOUSE_WHEEL) && !legacyWheel)
                io.AddMouseWheelEvent(0.0F,
                    static_cast<float>(static_cast<SHORT>(sample.buttonData)) /
                        static_cast<float>(WHEEL_DELTA));
            if ((sample.buttonFlags & RI_MOUSE_HWHEEL) && !legacyWheel)
                io.AddMouseWheelEvent(
                    static_cast<float>(static_cast<SHORT>(sample.buttonData)) /
                        static_cast<float>(WHEEL_DELTA), 0.0F);
        } else if (sample.type == RIM_TYPEKEYBOARD && !legacyKeyboard) {
            rawKeyboardObserved_ = true;
            FeedRawKey(window, sample);
        }
    }

    // When RIDEV_NOLEGACY suppresses WM_LBUTTONDOWN/UP, these physical
    // states still work. Avoid relying exclusively on button messages.
    constexpr int keys[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
    for (int i = 0; i < 5; ++i)
        io.AddMouseButtonEvent(i, (GetAsyncKeyState(keys[i]) & 0x8000) != 0);

    if (rawKeyboardObserved_ && !legacyKeyboard) {
        io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                                       (GetAsyncKeyState(VK_RWIN) & 0x8000));
    }
}
} // namespace m1core
