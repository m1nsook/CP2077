# m1core — native settings foundation (development)

**Status: prototype, not released or in-game verified.** The first milestone provides a real C++ RED4ext plugin entry point, a native REDscript settings API, typed settings, and JSON persistence. There is **no overlay yet**. The supplied HTML demo is a *layout reference*, not the final visual design.

## Architecture

- `red4ext/plugins/m1core/m1core.dll` — one native plugin; no separate DLL per feature mod.
- `r6/scripts/m1core/m1core.reds` — matching REDscript native declarations.
- `red4ext/plugins/m1core/settings.json` — generated on save, shared across mods. Keys are persistent CName hashes and isolated per mod.
- `src/settings.*` — pure C++ registry with bool / Int32 / Float, clamping, resets, and schema-v1 JSON.
- `src/plugin.cpp` — RED4ext lifecycle and registered native globals.

Settings registration must happen before reading or setting values. A getter for an unregistered setting returns `false`, `0`, or `0.0`; setter methods return `false` on failure. Call `M1Core_SaveSettings()` after changes when appropriate. The plugin also saves on unload. Persistent settings for temporarily absent mods are retained.

## Build

On Windows with Visual Studio 2022 and CMake 3.23 or newer, run from the repository root:

```powershell
cmake -S m1core -B m1core/build -A x64 -DBUILD_TESTING=ON
cmake --build m1core/build --config Release --parallel
ctest --test-dir m1core/build -C Release --output-on-failure
```

CMake downloads pinned RED4ext.SDK (revision `a34f0ad`, which defines game v2.50), and nlohmann/json v3.11.3. CI creates a development ZIP containing the DLL and the bindings at game-root-relative paths.

**Development prerequisite:** RED4ext, redscript compiler / redscript mod support, and Cyberpunk 2077 2.50. No CET or Mod Settings is required for the m1core library itself. Other mods may retain their own dependencies until migration.

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

1. **Milestone 1 (this branch):** native RED4ext plugin, versioned REDscript API, registry, persistence, CTest and Windows CI build.
2. **Milestone 2:** safe D3D12/ImGui renderer with reliable game/overlay input ownership. Escape, close and emergency recovery must work; never leave game input blocked.
3. **Milestone 3:** searchable sidebar/grouped mod settings, keybinds, profiles, visual customization independent of the HTML demo.
4. **Milestone 4:** live in-game validation, compatibility audit, test mod, release ZIP and API freeze.
5. **After m1core is stable:** migrate existing CP2077 mods individually, maintaining their working behavior.

No existing mod is changed by this milestone. Do not call m1core finished based on a successful build alone.
