# M1CORE — native RED4ext replacement for Cyber Engine Tweaks

**Status: unreleased development. Native Windows DLL build artifacts are now available from GitHub Actions; in-game behavior has not been verified.**

This project is a **standalone native Windows x64 DLL loaded by RED4ext**.
It must work on Cyberpunk 2077 without Cyber Engine Tweaks installed.
The previously published development artifact that required CET was a
mistake, has been invalidated, and is no longer the product design.

## Non-negotiable requirements

- \`red4ext/plugins/m1core/m1core.dll\` is the framework.
- CET **must not** be a runtime requirement, UI host, keybind provider,
  scripting host, cursor manager, or bundled component.
- Native Dear ImGui for the settings UI and user-selected hotkeys only;
  no hard-coded F-key.
- One game-ready ZIP when complete, with \`red4ext/\` at the root.
- Keep all other mods and the repository under MIT. M1CORE is MIT too.
- No process-global \`SetCursorPos\`, \`ClipCursor\`, \`ShowCursor\` detours,
  no raw-input mouse-speed multiplier, and no software cursor if Windows'
  engine-controlled cursor is present.
- Alt+F4, Alt+Tab, game shutdown, and second-monitor input ownership
  must be validated before a playable artifact is provided.

## Engineering foundation

Use the **proven architecture** of the source supplied for CET:
a game render-present hook, the actual D3D12 swapchain/command queue,
Dear ImGui with Win32/DX12 backends, and the REDengine ForceCursor API.

The native code must be independently implemented. CET is GPLv3 with
an exception for independent mods that use its public scripting interface;
that exception **does not** relicense copies of CET's C++ core. No CET
core code is copied into this MIT source.

The RED4ext SDK exposes \`sdk->hooking->Attach/Detach\` for game
functions. Use this instead of adding unnecessary hook libraries.
A missing game symbol should fail safely, leaving game input usable.

## Development status

- Old native overlay and CET-dependent Lua UI have been removed.
- Native DLL state persistence and RED4ext plugin entrypoint are present,
  with automated tests for atomic saves and corrupt-file protection.
- Renderer lifecycle, native UI, cursor ownership and hotkeys are **not
  complete**. The DLL is not a usable CET replacement yet.
- CI compiles/tests the native DLL and publishes a **single development
  ZIP** named `m1core-standalone-x64-dev`. Its only root folder is
  `red4ext/`, with `red4ext/plugins/m1core/m1core.dll` inside.
  It does not contain CET or require CET to build.
- **This downloadable build is not a gameplay-tested release.** DLL compilation
  and unit tests cannot establish startup, input, focus, cursor or rendering
  correctness inside Cyberpunk.

The old unreleased implementation remains in Git history and the
\`archive/m1core-before-cet-reset-20261120\` branch. Since m1core was
never released, no migration layer or compatibility bridge is required.

No changes are made to any other mod.
