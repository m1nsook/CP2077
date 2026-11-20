#pragma once

namespace m1core
{
class Registry;

// Draws the m1core settings view inside an existing ImGui frame.
// Does not hook D3D12, create an ImGui context or intercept input.
void RenderSettings(Registry& registry, bool& open);

// Render-thread callback when the overlay closes, also cancelling key capture.
void OnOverlayHidden(bool persisted);
// Thread-safe flag: menu-toggle must not interrupt the bind recorder.
bool IsCapturingKeybind();
} // namespace m1core
