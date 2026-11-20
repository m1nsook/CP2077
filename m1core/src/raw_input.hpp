#pragma once

#include <Windows.h>
#include <cstdint>
#include <vector>

namespace m1core
{
// Copy only safe scalar data from WM_INPUT on the HWND owner thread. Never
// store HRAWINPUT across threads: that handle may stop being valid.
struct RawSample {
    DWORD type = 0;
    LONG dx = 0;
    LONG dy = 0;
    USHORT mouseFlags = 0;
    USHORT buttonFlags = 0;
    USHORT buttonData = 0;
    USHORT vkey = 0;
    USHORT keyFlags = 0;
    USHORT makeCode = 0;
};

bool DecodeRawInput(LPARAM rawHandle, RawSample& out);

// Presses/movement are intercepted; releases must reach the game to prevent
// a key/mouse button held before opening the overlay remaining stuck.
bool ForwardToGame(const RawSample& sample);

class RawInputBridge {
public:
    void Reset() noexcept;

    // Feed legacy Win32 messages and copied RAWINPUT to ImGui, on the graphics
    // thread only, after the platform NewFrame and before ImGui::NewFrame.
    // Prefer real Win32 cursor coordinates, matching Windows multi-monitor
    // movement/speed. Use reduced-gain relative movement only as a fallback.
    void Feed(HWND window, const std::vector<MSG>& legacy,
              const std::vector<RawSample>& raw, bool systemCursor);

private:
    bool virtualActive_ = false;
    bool rawKeyboardObserved_ = false;
    bool positionReady_ = false;
    float mouseX_ = 0.0F;
    float mouseY_ = 0.0F;
};
} // namespace m1core
