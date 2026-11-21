# CET source supplied for m1core — November 21, 2026

Source: user-supplied `cet_m1nsook.zip`, SHA-256
`83a4488038e9c085e508d00b2bce8bf62c80baadcbfe92470aa25fa52469acc1`.
The ZIP was read from the conversation upload, not substituted with a
web-source download. Its 155 ZIP entries include `LICENSE-MIT`, authored
by yamashi (2018), and `ThirdParty_LICENSES`. The latter lists multiple
third-party notices, including Dear ImGui by Omar Cornut.

Source files inspected for THIS runtime feature and compared to
`m1core/native`:

| Donated CET file | m1core integration |
| --- | --- |
| `src/window/window.cpp` | Separate game HWND routing and input gate. Reconcile actual foreground each Present without requiring an Alt+Tab to generate focus messages. |
| `src/d3d12/D3D12.cpp` | Defer input capture until ImGui backend and REDengine ForceCursor are ready; preserve non-overlay game events and shutdown. |
| `src/d3d12/D3D12_Hooks.cpp` | Initialize from game Present, reset on resize/shutdown. |
| `src/d3d12/D3D12_Functions.cpp` | Bind Win32/DX12 backends to the game HWND, preserve ImGui context across resets, stage input/frame on the correct thread. |
| `src/imgui_impl/win32.cpp` | Respect the game client-to-render coordinate scale. m1core maps queued mouse coordinates onto the D3D12 backbuffer without importing the CET-specific backend. |
| `src/overlay/Overlay.cpp` | Set up first-time binding after game initialization, then release capture after binding. |
| `src/overlay/widgets/Bindings.cpp` | Use Dear ImGui `OpenPopup` + `BeginPopupModal` for built-in dimming/focus. |
| `src/VKBindings.cpp` | Handle key edges, including raw keyboard events when the game suppresses legacy Win32 key messages. |

This source is used as a concrete, user-provided architectural reference.
The DLL remains standalone RED4ext, with no required CET installation,
Lua VM, CET internal global dependencies or copied CET source. Existing
m1core remains MIT. The source ZIP's MIT notice alone is not assumed to
relicense any independently copyrighted third-party component.

Current limitation: CI checks code and packaging, not Cyberpunk's live
focus/mouse behavior. Keep PR #40 in draft until real runtime confirmation.


### Donated source used directly in a compiled module

`native/window_discovery.cpp` now ports the original `src/window/window.cpp` foreground game-window discovery behavior: `EnumWindows`, process ID matching and exact `W2ViewportClass` detection. The search is invoked by the RED4ext render bridge before the graphics-frame discovery stage, to avoid coupling early HWND binding to an already functioning DX12 renderer. Unlike the old detached-thread CET bootstrapping, this runs inside the existing RED4ext Present callback and leaves gameplay input passive until successful overlay capture. The original author copyright statement is retained in the MIT license file packaged with the DLL.

The source archive also contains bundled assets and an ASI loader (`src/common/FontMaterialDesignIcons.h`, `vendor/asiloader/version.dll`), as well as separate third-party licenses. Those files are **excluded**, and this project does not copy unrelated CET implementation files or fonts/icons. Asset reuse is not authorized merely by the base MIT notice.
