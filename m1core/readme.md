# m1core — native settings foundation (development)

**Status: experimental v0.2.0-dev, not released.** Cyberpunk 2.50, RED4ext 1.31.1 and redscript startup were verified on 2026-11-19. User's opt-in smoke test returned **0 for both phases**, verifying registry API and restart persistence. The first D3D12 ImGui renderer is now included, but is **not yet in-game verified**. The supplied HTML demo is a *layout reference*, not the final visual design.

## Architecture

- `red4ext/plugins/m1core/m1core.dll` — one native plugin; no separate DLL per feature mod.
- `r6/scripts/m1core/m1core.reds` — matching REDscript native declarations.
- `red4ext/plugins/m1core/settings.json` — generated on save, shared across mods. Keys are persistent CName hashes and isolated per mod.
- `src/settings.*` — pure C++ registry with bool / Int32 / Float, clamping, resets, and schema-v1 JSON.
- `src/plugin.cpp` — RED4ext lifecycle and registered native globals.
- `src/ui.cpp` — Dear ImGui view layer with a searchable mod sidebar, bool/int/float controls and save/reset. The opt-in D3D12 overlay can render this view; visual/UI behavior requires in-game verification.
-  `src/overlay.cpp` — experimental DXGI Present/Resize hooks, game-window subclass, native Win32/D3D12 ImGui renderer, emergency-disable shortcut; opt-in only.
- `src/cursor.cpp` — opt-in guarded user32 cursor release / recenter suppression while the overlay is open, restores the game's clip on close or focus loss; detour install failure leaves the virtual mouse fallback available.
- `src/raw_input.cpp` — copies RAWINPUT on the window thread and injects relative mouse movement, button presses, wheel, raw-key input and text into ImGui on the render thread. It does not depend on the game exposing legacy WM_MOUSEMOVE / WM_LBUTTONDOWN events.
- `tests/ingame/m1core_smoke.reds` — opt-in native API and persistence smoke test. **Not** shipped in the normal ZIP.

Settings registration must happen before reading or setting values. A getter for an unregistered setting returns `false`, `0`, or `0.0`; setter methods return `false` on failure. Call `M1Core_SaveSettings()` after changes when appropriate. The plugin also saves on unload. Persistent settings for temporarily absent mods are retained.

## Build

On Windows with Visual Studio 2022 and CMake 3.23 or newer, run from the repository root:

```powershell
cmake -S m1core -B m1core/build -A x64 -DBUILD_TESTING=ON
cmake --build m1core/build --config Release --parallel
ctest --test-dir m1core/build -C Release --output-on-failure
```

CMake downloads pinned RED4ext.SDK (revision `a34f0ad`, which defines game v2.50), nlohmann/json v3.11.3, and Dear ImGui v1.92.4 including the Win32 and D3D12 backends. CI creates a development ZIP containing the DLL and the bindings at game-root-relative paths.

**Development prerequisite:** RED4ext, redscript compiler / redscript mod support, and Cyberpunk 2077 2.50. No CET or Mod Settings is required for the m1core library itself. Other mods may retain their own dependencies until migration.

## Opt-in ImGui overlay preview — v0.2.0-dev

**New cursor/input build not yet verified in-game. Do not install during important gameplay.** The usual `m1core-windows-x64-dev` package is still **headless**: it does not install `overlay.enable`, so it never creates graphics/window hooks.

For a first UI test, GitHub Actions publishes **`m1core-overlay-preview-x64-dev`** as a separate artifact. It includes an `overlay.enable` file which explicitly enables the experimental DXGI Present/Resize renderer. The overlay adds temporary sample settings under `m1core.preview` to demonstrate the sidebar, toggle, integer slider and float slider before other mods migrate.

- **F11** — toggle the settings window after it has initialized.
- **Escape** — close immediately.
- **Ctrl+Shift+F11** — emergency disable until game restart.
- **Recovery before next launch:** delete `red4ext/plugins/m1core/overlay.enable`. This fully disables our graphics and window hooks, without removing m1core itself.
- **Full uninstall:** remove the DLL and `m1core.reds` together.

The overlay draws a dimmed fullscreen backdrop via ImGui's background draw list and shows normal draggable ImGui settings windows on top. It tracks raw-relative mouse deltas to draw an ImGui cursor independently of the game cursor; it also attempts to guard user32 `ClipCursor` / `SetCursorPos` while capturing input, restoring the last requested clip when the overlay closes. Key/button releases continue to reach the game to avoid stuck controls.

The overlay has fail-closed focus rules and releases back-buffer references before swap-chain resize. It only hooks when the RED4ext Running callback executes on the owner thread of the game window; if this thread check fails, it **does not** attach hooks. Normal builds contain the renderer code but leave it inactive.

**Known limitations:** this cursor rewrite has not been verified in-game. The user32 hooks are process-wide while the preview is enabled, though they act only during m1core capture; they may interact with other overlay mods. Virtual cursor movement works from relative RAWINPUT even if OS cursor recenter suppression fails. Non-US keyboard layouts, IME composition, controller navigation, and third-party overlay coexistence still need testing. If the UI is visible but input behaves unexpectedly, press the emergency shortcut, exit the game, delete `overlay.enable` and send logs. Do not treat a successful CI build as proof that real input behavior is correct.

## Optional in-game native API test

The CI workflow produces a second, separately labeled artifact `m1core-smoke-test-x64-dev`. It includes the same DLL/bindings plus the test-only `m1core_smoke.reds` helper. The normal build does not include the helper.

1. When ready to test, install the **smoke-test ZIP**, not both ZIPs, into the game root and load a save.
2. In CET console run `print(Game.GetPlayer():M1Core_SmokeTest(1))`. Expected result is **0**. It tests native registration/getters/setters/reset and writes test values to `red4ext/plugins/m1core/settings.json`.
3. Fully close the game, relaunch, and load the save. Run `print(Game.GetPlayer():M1Core_SmokeTest(2))`. Expected result is **0** if saved values survived restart.
4. Delete `r6/scripts/m1core/m1core_smoke.reds` after testing; the test does not change gameplay, inventory, or quest state.

A nonzero result is a failure bitmask and should be reported with the m1core/RED4ext/redscript logs. This test is opt-in because CET is used only as a test console, never as an m1core runtime dependency.

## API example

```redscript
// Example only, not installed by this project.
let ok = M1Core_RegisterFloatSetting(n"ExampleMod", n"Speed", 1.0, 0.5, 5.0);
let speed = M1Core_GetFloatSetting(n"ExampleMod", n"Speed");
let changed = M1Core_SetFloatSetting(n"ExampleMod", n"Speed", 2.0);
M1Core_SaveSettings();
```

The bindings are installed only with their matching DLL. Never ship the `.reds` file alone.

## Roadmap

1. **Milestone 1 (compiled and launched):** native RED4ext plugin, versioned REDscript API, registry, persistence, CTest and Windows CI build; optional API/persistence runtime test awaiting results.
2. **Milestone 2 (experimental):** opt-in D3D12/ImGui render path, guarded Win32 input, three exit mechanisms, game focus and resize handling. Still requires in-game verification. Escape, close and emergency recovery must work; never leave game input blocked.
3. **Milestone 3:** searchable sidebar/grouped mod settings, keybinds, profiles, visual customization independent of the HTML demo.
4. **Milestone 4:** live in-game validation, compatibility audit, test mod, release ZIP and API freeze.
5. **After m1core is stable:** migrate existing CP2077 mods individually, maintaining their working behavior.

No existing mod is changed by this milestone. Do not call m1core finished based on a successful build alone.
