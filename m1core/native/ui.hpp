#pragma once
namespace m1core {
// The ImGui drawing function runs only on the game render thread.
// It neither reads global keyboard state nor hooks Win32 input.
void DrawNativeSettingsUI();
} // namespace m1core
