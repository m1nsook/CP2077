# m1core — native settings foundation (development)

**Status: prototype, not released.** Cyberpunk 2.50, RED4ext 1.31.1 and redscript startup were verified in-game with v0.1.0 on 2026-11-19. The native API has not yet been called from REDscript in-game, and there is **no visible overlay yet**. The supplied HTML demo is a *layout reference*, not the final visual design.

## Architecture

- `red4ext/plugins/m1core/m1core.dll` — one native plugin; no separate DLL per feature mod.
- `r6/scripts/m1core/m1core.reds` — matching REDscript native declarations.
- `red4ext/plugins/m1core/settings.json` — generated on save, shared across mods. Keys are persistent CName hashes and isolated per mod.
- `src/settings.*` — pure C++ registry with bool / Int32 / Float, clamping, resets, and schema-v1 JSON.
- `src/plugin.cpp` — RED4ext lifecycle and registered native globals.
- `src/ui.cpp` — Dear ImGui view layer with a searchable mod sidebar, bool/int/float controls and save/reset. This is not rendered in-game until D3D12 presentation and focus handling are implemented.
- `tests/ingame/m1core_smoke.reds` — opt-in native API and persistence smoke test. **Not** shipped in the normal ZIP.

Settings registration must happen before reading or setting values. A getter for an unregistered setting returns `false`, `0`, or `0.0`; setter methods return `false` on failure. Call `M1Core_SaveSettings()` after changes when appropriate. The plugin also saves on unload. Persistent settings for temporarily absent mods are retained.

## Build

On Windows with Visual Studio 2022 and CMake 3.23 or newer, run from the repository root:

```powershell
cmake -S m1core -B m1core/build -A x64 -DBUILD_TESTING=ON
cmake --build m1core/build --config Release --parallel
ctest --test-dir m1core/build -C Release --output-on-failure
```

CMake downloads pinned RED4ext.SDK (revision `a34f0ad`, which defines game v2.50), nlohmann/json v3.11.3, and Dear ImGui v1.92.4. CI creates a development ZIP containing the DLL and the bindings at game-root-relative paths.

**Development prerequisite:** RED4ext, redscript compiler / redscript mod support, and Cyberpunk 2077 2.50. No CET or Mod Settings is required for the m1core library itself. Other mods may retain their own dependencies until migration.

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
2. **Milestone 2 (in development):** settings view ready; safe D3D12/ImGui renderer with reliable game/overlay input ownership. Escape, close and emergency recovery must work; never leave game input blocked.
3. **Milestone 3:** searchable sidebar/grouped mod settings, keybinds, profiles, visual customization independent of the HTML demo.
4. **Milestone 4:** live in-game validation, compatibility audit, test mod, release ZIP and API freeze.
5. **After m1core is stable:** migrate existing CP2077 mods individually, maintaining their working behavior.

No existing mod is changed by this milestone. Do not call m1core finished based on a successful build alone.
