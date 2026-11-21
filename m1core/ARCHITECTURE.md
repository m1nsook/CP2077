# Native rendering and input contract

Source of architecture: CyberEngineTweaks master C++ sources (especially
\`src/d3d12/D3D12_Hooks.cpp\`, \`D3D12_Functions.cpp\`,
\`src/d3d12/D3D12.cpp\`, \`src/window/window.cpp\` and
\`src/reverse/Addresses.h\`). The source is GPLv3, so use independent MIT
implementation of observable public engine interfaces, not copied CET code.

Standalone m1core must:

1. Be loaded exclusively through RED4ext's Plugin API.
2. Hook the game's rendering lifecycle through
   \`RED4ext::v1::Sdk::hooking\` (not CET, not a separate injected frontend).
3. Resolve game addresses from RED4ext's address resolver and fail closed
   if the specific game version does not support them.
4. Acquire DX12 device, queue and swapchain from Cyberpunk's actual
   renderer; initialize only after those are valid.
5. Own one Dear ImGui context and Win32/DX12 backend; cleanly release
   GPU resources on resize/shutdown and game unload.
6. Only capture keyboard/mouse for the owned game HWND when it is actually
   foreground and the UI is explicitly open. Do not capture on game startup
   prior to user activation or the legitimate first-run key configuration.
7. Request/release Cyberpunk's native cursor by a distinct reason token.
   Do not use Win32 cursor detours or software cursors if the engine shows
   the real pointer.
8. Always preserve Alt+F4, SC_CLOSE, WM_CLOSE, OS shortcuts, Alt+Tab and
   foreground focus changes. No event polling outside the game HWND.
9. Use a native first-run keybind window with user-chosen toggle key,
   persisted to disk, without overriding the user's existing CET key.
10. Use **only stock Dear ImGui widgets and its default dark style** for the
    current stage: a normal window, BeginTabBar/BeginTabItem, BeginChild,
    BeginTable, Selectable, SliderInt/Float, Checkbox, BeginCombo and Button.
    Do not reintroduce colored navigation, custom draw lists, fixed-position
    widget hit areas, custom fonts, or pixel-perfect Photoshop designs.
11. Test render lifecycle, focus rules, hotkeys and UI layout with
    native automated tests. CI success does not replace real in-game QA.

This file describes the intended standalone design. It does not claim
those capabilities are already implemented.


## Current implementation phase (dev.3)

The independent native implementation is being connected. These sources
replace the CET-dependent prototype, not augment it:

- \`native/render_bridge.*\`: hooks the game's internal Present, Resize and
  Shutdown through **RED4ext SDK Hooking**, with graceful resolver failure.
- \`native/renderer.*\`: private Dear ImGui Win32 + DirectX 12 context,
  device, swapchain and command-queue integration, its own descriptor heaps,
  command allocators and per-backbuffer GPU fences.
- \`native/input.*\`: foreground-only Win32 message routing and native
  REDengine \`ForceCursor\` ownership, one physical pointer and no API detours.
  OS shutdown commands remain pass-through.
- \`native/ui.*\`: stock Dear ImGui tabs, setting tables, default dark style and
  typed settings backed directly by the independent native registry.
- \`native/plugin.cpp\`: new native RED4ext RTTI access to register, read,
  write and reset settings. **No CET Lua files or CET installation.**

### CET source audit (November 21, 2026)

Compared independently against `maximegmd/CyberEngineTweaks` master:
`src/window/window.cpp`, `src/d3d12/D3D12.cpp`,
`src/d3d12/D3D12_Hooks.cpp`, `D3D12_Functions.cpp`,
`src/VKBindings.cpp`, `src/overlay/Overlay.cpp`, and
`src/reverse/RenderContext.h`. CET constructs its ImGui backend from the
render Present lifecycle, enables gameplay input trapping only when the
overlay is active, and starts first-time binding after initialization.
On resize, CET resets backends without destroying the ImGui context.
Its overlay uses ordinary ImGui windows and a passthrough dockspace; it
does not require a hand-drawn fullscreen darkening layer.

M1CORE now passively initializes on a valid Present without swallowing
startup keys, explicitly arms first-run setup after backend initialization,
retains its ImGui context across resize, derives key-down/up edges from
raw keyboard input with duplicate Win32-event suppression, and drops input
ownership when GPU frames stall. These are independently written RED4ext/ImGui integrations,
not CET code or a CET runtime requirement. M1CORE's render-thread UI is
still a difference from CET's main-thread UI preparation plus draw-buffer
staging and needs in-game compatibility evaluation.

### RED4ext lifecycle, first-run keyboard, and user theme (November 21)

m1core now initializes native graphics on a valid Present regardless of
foreground focus, but draws/captures only for the foreground game HWND.
RED4ext's BaseInitialization game-state callback gates automatic first-run
setup. This mirrors CET's post-initialization first-time-bind behavior,
without depending on CET or guessing startup Enter/Space input.

Keyboard-only first-run recording no longer requires the engine mouse
cursor. The game HWND listener also installs without requiring
`ForceCursor` to be resolvable, so users may select a hotkey even when
the game mouse cursor is late or unavailable. Focus regain re-arms a
first-run recorder paused by Alt+Tab. An absent native cursor always
disables general ImGui mouse and keyboard ownership. After the first successfully submitted native ImGui frame,
the user-chosen binding is captured on a physical key-release edge.
General ImGui keyboard/mouse input continues to require the owned
REDengine cursor, and focus/shutdown protection remains in place.

The user-supplied `ImGuiCustomTheme.h` is applied to normal Dear ImGui
style colors and spacing when the context is first created. This is
an **ImGui style**, not hand-drawn controls or custom UI widgets.
The theme does not change the renderer or input ownership design.

### First-run input recovery (November 21)

User confirmed game startup still worked but the first-run menu appeared
only after repeated focus changes, and keyboard input was not usable.
The old implementation required clicking a **Bind menu key** button
before recording, and also permanently closed setup if REDengine's
ForceCursor was temporarily unavailable during initialization.

The revised setup records the first valid key directly, automatically
resumes recording after focus restoration, retries acquiring the native
cursor on subsequent valid Present frames, and never captures game input
without owning the cursor and an initialized ImGui renderer. RED4ext logs
explicit milestones for render hook installation, discovery of
swapchain/queue/HWND, ImGui backend initialization, first draw submission,
and cursor acquire/release. No startup key guessing or CET runtime.

The game symbol hashes and render-layout offsets must be verified against
the target Cyberpunk build. Unknown symbols must leave the plugin inert
rather than terminating Cyberpunk. Native compilation and unit tests are
necessary but **insufficient for release**. The user will not test
intermediate builds. Do not upload or label any developer build as an
installable CET replacement until graphics lifecycle, first-run key
binding, cursor ownership, shutdown and non-game focus handling receive
end-to-end verification.

### UI decision (November 21, 2026)

The user explicitly requested a **plain standard ImGui interface** during the standalone restart. The custom colored-header reference is not the active design target. Prioritize reliability and functionality; do not return to custom UI controls unless the user explicitly changes direction. Keep the RED4ext DLL standalone without CET.
