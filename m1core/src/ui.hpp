#pragma once

namespace m1core
{
class Registry;

// Draws the m1core settings view inside an existing ImGui frame.
// Does not hook D3D12, create an ImGui context or intercept input.
void RenderSettings(Registry& registry, bool& open, bool interactive);

// Render-thread callback when the overlay closes, also cancelling key capture.
void OnOverlayHidden(bool persisted);
// Called when game loses focus: cancel any pending keybind recording, so
// Alt+Tab and keyboard activity on other monitors cannot configure m1core.
void OnOverlayFocusLost();
// Thread-safe flag: menu-toggle must not interrupt the bind recorder.
bool IsCapturingKeybind();
} // namespace m1core
