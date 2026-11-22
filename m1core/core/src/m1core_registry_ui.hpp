#pragma once
namespace m1core {
// Dear ImGui registry frontend; assumes the donated CET overlay has already
// opened the ImGui frame and acquired keyboard/mouse input.
void DrawNativeRegistryUI();
// Signal the overlay's actual open transition; opens compatibility modal on
// the next ImGui frame if incompatible mods were found.
void NotifyOverlayOpened();
} // namespace m1core
