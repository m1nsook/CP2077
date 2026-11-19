#pragma once

namespace m1core
{
class Registry;

// Draws the m1core settings view inside an existing ImGui frame.
// Does not hook D3D12, create an ImGui context or intercept input.
void RenderSettings(Registry& registry);
} // namespace m1core
