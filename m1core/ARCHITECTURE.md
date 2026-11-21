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
10. Use normal ImGui controls, consistent fonts and spacing, with the
    user's chosen black/white/colored-tab design.
11. Test render lifecycle, focus rules, hotkeys and UI layout with
    native automated tests. CI success does not replace real in-game QA.

This file describes the intended standalone design. It does not claim
those capabilities are already implemented.
