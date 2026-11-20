# m1core M5 — CET input review and fail-safe redesign

**Status:** Native compilation/tests only until the player verifies a game run.
**Source reviewed:** the user-uploaded \`CyberEngineTweaks-master(1).zip\`,
188 archive entries, 125 C/C++ source/header files (~37K lines), plus
build/license files. We mapped the full tree and traced the interdependent
window, input, graphics, overlay and keybinding subsystems:

- \`src/window/window.cpp\` and \`src/window/window.h\`
- \`src/d3d12/D3D12.cpp\`, \`D3D12_Hooks.cpp\`, \`D3D12_Functions.cpp\`
- \`src/overlay/Overlay.cpp\` and \`Overlay.h\`
- \`src/VKBindings.cpp\` and \`VKBindings.h\`
- \`src/imgui_impl/win32.cpp\`, its ImGui backend and the RED4ext relocation API
- \`src/reverse/Addresses.h\`, game engine \`InputSystemWin32Base::ForceCursor\`

## Key observations

CET asks the **Cyberpunk engine** for cursor ownership with a reason token,
rather than replacing process-wide Win32 \`SetCursorPos\` and \`ClipCursor\`
functions or integrating raw mouse deltas into a software pointer. It
processes overlay keyboard input through its window handler, while recording
state is cancelled on focus loss. The game's RAWINPUT registration must be
considered along with legacy window messages; ALT+F4 can be suppressed by
\`RIDEV_NOLEGACY\` before normal WM_SYSKEYDOWN processing.

m1core previously swallowed WM_SYSKEYDOWN while the overlay was active,
polled \`GetAsyncKeyState\` during background rendering, and installed global
user32 cursor hooks. This combination explains the user's trapped mouse,
broken shutdown and cross-monitor key bleed better than arbitrary cursor
"speed" multipliers.

## Implemented as independent MIT code

- Resolve the engine cursor function gracefully from RED4ext at gameplay
  initialization. No fatal SDK relocation resolver, injected window hooks
  on cursor APIs, raw mouse movement scaling or display-counter overrides.
- Apply/release cursor on the HWND thread through a registered window message.
  Relinquish the reason token on focus loss, close, Alt+F4 and shutdown.
  A missing engine function disables the overlay safely.
- Never poll hotkey state or intercept RAWINPUT when Cyberpunk is not the
  desktop foreground. Cancel keybind recording on focus loss *even if no
  frame was rendered until after focus returned*.
- Preserve both legacy Alt+F4 and RAWINPUT Alt+F4, and SC_CLOSE/WM_CLOSE.
  All routes release overlay ownership and hand the close back to Windows.
- Fullscreen UI: explicit positions for fixed header, nav, body and footer
  prevent ImGui ItemSpacing from overflowing the viewport; flat sidebar
  separator and underline-only keyboard/selected navigation focus.
- Group order follows the submitted final.html: General, Gameplay, Controls.
  No default F-key binding is introduced.

## License boundaries

\`LICENSE\` in the uploaded CET source is GPLv3, and \`LICENSE-EXCEPTION\`
does **not** permit copying CET core source into a separately MIT-licensed
framework. \`LICENSE-MIT\` relates to contributions before the CET project
changed its license and does not relicense current GPL CET source wholesale.

This change uses the observable API, address hash/engine semantics and input
requirements to implement independent m1core code. It does **not** import
CET source into the m1core repo or alter its MIT license.

## Runtime signoff remains necessary

CI can verify compilation, simulated layout bounds, focus state logic, normal
and raw Alt+F4 classification, archive structure and included fonts. Only an
actual Cyberpunk session can prove: no input after Alt+Tab, native pointer
speed/acceleration, working Alt+F4 and manual game quit, second-monitor use,
hotkey persistence, renderer teardown and close/reopen behavior. Do not
call these gameplay issues resolved until the user confirms them.
