# m1core — native settings foundation (development)

**Status: experimental v0.5.0-dev (M5 in progress), not released.** Cyberpunk 2.50, RED4ext 1.31.1 and redscript startup were verified on 2026-11-19. User's opt-in smoke test returned **0 for both phases**, verifying registry API and restart persistence. The F11 D3D12 ImGui overlay and cursor movement were **confirmed in-game**; enum dropdowns, tabs, and keybind persistence have also been confirmed in-game. The supplied HTML demo is a *layout reference*, not the final visual design.

## Architecture

- `red4ext/plugins/m1core/m1core.dll` — one native plugin; no separate DLL per feature mod.
- `r6/scripts/m1core/m1core.reds` — matching REDscript native declarations.
- `red4ext/plugins/m1core/settings.json` — generated on save, shared across mods. Keys are persistent CName hashes and isolated per mod.
- `red4ext/plugins/m1core/profiles.json` — separately managed named configuration snapshots; never included in an install ZIP, so updates do not overwrite user-created profiles.
- `src/settings.*` — pure C++ registry with bool / Int32 / Float / enum / keybind (Win32 VK), clamping, resets, and schema-v1 JSON.
- `src/plugin.cpp` — RED4ext lifecycle and registered native globals.
- `src/ui.cpp` — Dear ImGui view layer with a searchable mod sidebar, bool/int/float controls, enum dropdowns, keybind capture, functional All Mods / Keybinds / Profiles / About tabs, and save/reset. The opt-in D3D12 overlay can render this view; the previous dropdowns and keybinds were tested in-game; profiles and per-mod grouping were confirmed in-game on 2026-11-19.
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

CMake downloads pinned RED4ext.SDK (revision `a34f0ad`, which defines game v2.50), nlohmann/json v3.11.3, and Dear ImGui v1.92.4 including the Win32 and D3D12 backends. CI uploads one game-ready artifact ZIP with `r6/` and `red4ext/` directly at its root; there are no inner ZIPs.

**Development prerequisite:** RED4ext, redscript compiler / redscript mod support, and Cyberpunk 2077 2.50. No CET or Mod Settings is required for the m1core library itself. Other mods may retain their own dependencies until migration.

## Opt-in ImGui overlay preview — v0.4.0-dev

**Experimental development build.** The F11 overlay, cursor, enum dropdowns, keybind capture and persistence were confirmed working in-game on 2026-11-19. This single development package includes `overlay.enable` for convenient UI testing. Remove that marker to run the settings API without graphics/input hooks.

For testing, download the single **`m1core-game-ready-x64-dev`** artifact from the latest successful GitHub Actions run **by clicking the artifact name, not "Download all"**. GitHub already delivers a ZIP, so extract it **once** into the Cyberpunk 2077 game root. The ZIP begins directly with `r6/` and `red4ext/`; there is no extra wrapper folder, nested ZIP, or manual assembly.

The dev ZIP includes the native DLL, matching REDscript API, optional smoke-test helper, and `overlay.enable`. The smoke helper performs no actions until explicitly invoked from a test console. It also includes temporary sample settings under `m1core.preview` to demonstrate bool/int/float sliders, an enum dropdown and keybind capture before mods migrate.

- **F11** — open or close the settings window with the same key.
- **Ctrl+Shift+F11** — emergency disable until game restart.
- **Recovery before next launch:** delete `red4ext/plugins/m1core/overlay.enable`. This fully disables our graphics and window hooks, without removing m1core itself.
- **Full uninstall:** remove the DLL and `m1core.reds` together.

The overlay draws a dimmed fullscreen backdrop via ImGui's background draw list and shows normal draggable ImGui settings windows on top. It tracks raw-relative mouse deltas to draw an ImGui cursor independently of the game cursor; it also attempts to guard user32 `ClipCursor` / `SetCursorPos` while capturing input, restoring the last requested clip when the overlay closes. Key/button releases continue to reach the game to avoid stuck controls.

The overlay has fail-closed focus rules and releases back-buffer references before swap-chain resize. It only hooks when the RED4ext Running callback executes on the owner thread of the game window; if this thread check fails, it **does not** attach hooks. Normal builds contain the renderer code but leave it inactive.

**In-game test:** the user confirmed the overlay and cursor controls work on 2026-11-19. F11 is the only normal keyboard open/close shortcut; Escape is not handled by m1core.

**Multi-monitor input follow-up (requires in-game testing):** The normal overlay path now uses the physical Windows cursor and its system pointer speed, instead of a separately drawn ImGui pointer. Cursor visibility changes run on the game window thread so the per-thread Windows ShowCursor counter is balanced on opening, focus loss, and closing. Clicking another monitor temporarily suspends m1core input but preserves the open menu; returning to the game restores interaction, and F11 still closes it. The raw-relative painted pointer remains only as a reduced-speed fallback if cursor detours cannot be installed. Automated input-gate tests cover focus preservation, but multi-monitor runtime behavior must be checked before this is considered fixed.

**Known limitations:** cursor movement and basic overlay behavior work in-game, but broader compatibility tests are pending. The user32 hooks are process-wide while the preview is enabled, though they act only during m1core capture; they may interact with other overlay mods. Virtual cursor movement works from relative RAWINPUT even if OS cursor recenter suppression fails. Non-US keyboard layouts, IME composition, controller navigation, and third-party overlay coexistence still need testing. If the UI is visible but input behaves unexpectedly, press the emergency shortcut, exit the game, delete `overlay.enable` and send logs. Do not treat a successful CI build as proof that real input behavior is correct.

## Named settings profiles (v0.4.0 development)

The **Profiles** tab provides named configurations, such as `Stealth`, `Combat`, and `Exploration`. Each profile snapshots the current values of all registered settings plus any previously persisted settings from temporarily absent mods. Values are matched by mod ID, setting ID and type, and are clamped to the currently declared bounds on restore.

- **Create from current settings:** Save a new name. The UI refuses duplicate names instead of silently overwriting.
- **Load selected:** Apply saved values immediately and write `settings.json` so they survive restart.
- **Overwrite selected:** Replace the saved snapshot only after a second confirmation click.
- **Delete selected:** Delete only that named snapshot after confirmation; it does not change currently active mod settings.
- **Rename selected (M5):** Rename a snapshot without changing its values or the active settings. Existing profile names cannot be overwritten. Changes are atomically written to `profiles.json`.
- **F11 to close:** Continues to save active settings automatically.

Profiles are stored in a separate `profiles.json` beside `settings.json`, using a versioned schema and temporary-file replacement. Up to 24 profiles are supported; names may use UTF-8 (up to 64 bytes) but cannot contain control characters or path separators. A damaged or unexpectedly large profiles file does not prevent loading the plugin or the original settings.

**Status:** named profiles were confirmed working in-game on 2026-11-19. Individual overwrite, deletion, and corrupted-file recovery paths are additionally covered by unit tests but should be rechecked in-game before release. The install package never includes `settings.json` or `profiles.json`, preserving both during updates.

## Mod-defined settings groups (v0.4.0 development)

A compatible mod can optionally label its settings with categories without changing their type, persisted value or CName ID. The native API must be called **after** registration:

```redscript
M1Core_RegisterBoolSetting(n"ExampleMod", n"Enabled", true);
M1Core_RegisterFloatSetting(n"ExampleMod", n"Intensity", 1.0, 0.5, 3.0);
M1Core_SetSettingGroup(n"ExampleMod", n"Enabled", n"General");
M1Core_SetSettingGroup(n"ExampleMod", n"Intensity", n"Gameplay");
```

The All Mods and Keybinds pages render categories in named sections. If a mod does not set a category, settings stay in the default General section. Group metadata is **not** written to `settings.json`: the mod declares categories again when it registers settings on the next startup. A later registration preserves categories already supplied during that process.

The development preview now demonstrates General, Gameplay and Controls groups. The grouping controls and Profiles tab were confirmed working in-game on 2026-11-19.

## M5 — advanced mod integration (v0.5.0-dev, first increment)

Three optional native APIs make registered settings easier to integrate with
m1nsook mods without changing their stable CName IDs or saved values:

- `M1Core_SetSettingDisplayName(modName, settingId, text)` — separate
  player-facing label (up to 96 UTF-8 bytes, empty clears the override).
- `M1Core_SetSettingDescription(modName, settingId, text)` — hover help
  (up to 512 UTF-8 bytes, empty clears it). Control characters are rejected.
- `M1Core_GetSettingRevision(modName, settingId)` — session-only
  integer token, initially 0, advancing only when the *effective* value
  changes. Returns **-1** for unregistered IDs. Mods may compare the last
  observed token and read their typed value only when it changes.

The revision covers changes through the UI, typed setters, individual and bulk
resets, profile activation, and re-registration that clamps a value because
the declared range changed. No-op writes and repeated profile activation do
not increment the token. Revisions start at 0 each game launch; they are not
saved, and are not a durable event queue. Token changes are **polled**, not
automatic REDscript callbacks.

Example:

```redscript
M1Core_RegisterFloatSetting(n"MyMod", n"Intensity", 1.0, 0.5, 3.0);
M1Core_SetSettingGroup(n"MyMod", n"Intensity", n"Gameplay");
M1Core_SetSettingDisplayName(n"MyMod", n"Intensity", "Effect Intensity");
M1Core_SetSettingDescription(n"MyMod", n"Intensity", "Controls the strength of this effect.");
// Store this number after registration. Compare it in your mod's update loop:
let revision: Int32 = M1Core_GetSettingRevision(n"MyMod", n"Intensity");
```

Metadata must be declared **after registration each startup**. It is deliberately
absent from `settings.json` and `profiles.json`. Existing mods work
without calling these new functions. The built-in `m1core.preview` now
demonstrates a renamed setting and a tooltip in the All Mods interface.

For optional native smoke testing, call
`print(Game.GetPlayer():M1Core_M5SmokeTest())` in a development
console if one is installed. Return **0** means registration, metadata,
revision and reset checks succeeded. This console is only a test mechanism,
not a required dependency of m1core.

### M5 native keybind actions (polling API)

`M1Core_PollKeybindPressed(modName, settingId)` returns true once per
fresh key press of that mod's **registered Keybind setting**. Example:

```redscript
// Register once on initialization:
M1Core_RegisterKeybindSetting(n"ExampleMod", n"QuickAction", 117); // F6
// In the mod's regular game update (call every tick, not only when held):
if M1Core_PollKeybindPressed(n"ExampleMod", n"QuickAction") {
    // Run the mod's own action here.
}
```

- No CET dependency and no Win32 keyboard hook needed in the consuming mod.
- Keybind values already saved in settings.json are used as-is.
- First call primes the current state (a key already held will not fire).
- Repeats and held keys produce no extra event; release and repress to fire.
- The call is suppressed while the m1core window is open or Cyberpunk is not
  the foreground application; overlay/focus changes invalidate held keys.
- F11 is always reserved. Unbound keys and unregistered settings never fire.
- Other mods binding the same VK are independent; collisions are not
  automatically resolved.
- **Current limit:** this is *edge polling*, not an asynchronous event queue.
  Mods must call every game update. A very short press entirely between two
  polls may be missed. A future native event dispatcher can address this.

The internal state machine has dedicated CTest coverage for held keys, repeats,
rebinds, focus suppression, reserved keys, and independent mod actions.

**Remaining M5 work:** native action event buffering/dispatch, improved mod
lifecycle metadata, and further profile-management UX. Do not mistake the
new polling revision API for a full gameplay event system.

### Carry-weight sample mod (opt-in game integration)

The dev ZIP includes a first real m1core consumer at
`r6/scripts/m1core_sample_weight/m1core_sample_weight.reds`.
Source, full installation and uninstall notes, and in-game test checklist:
`samples/carry_weight/README.md`.

- Registers Enabled, Carry Weight Limit (100–10,000, default 500 total),
  and a rebindable Toggle Mod key (F8 by default).
- Applies only its own tracked additive CarryCapacity modifier; it does
  not wipe stat modifiers owned by cyberware, perks or other mods.
- UI values are stored in m1core settings.json; the modifier is recreated
  after player attachment/load and periodically reconciled against gameplay
  stat rebuilds. F8 toggles and saves immediately through the new API.
- **New in-game behavior is unverified until tested inside Cyberpunk.**
  CI verifies the DLL and single-ZIP structure, not REDscript game compilation.

To remove just this sample, delete the sample .reds from the game root
and restart. Existing JSON settings/profiles are never overwritten.

## Native diagnostic log and in-game Logs panel (post-v0.4.0 development)

The new **Logs** tab shows the last 512 diagnostic events, with source/message
search, a minimum severity filter, clipboard copy, and a clear-panel button.
A separate on-disk log is kept at `red4ext/plugins/m1core/m1core.log`
and rotates at roughly 2 MiB to `m1core.log.previous`.
Clearing the panel does **not** erase either disk log. Native startup,
settings/profile load errors, and unload errors are journaled.

m1nsook mods may write their own events with the REDscript native API:

```redscript
M1Core_LogEvent(n"ExampleMod", 1, "Initialized"); // Info
M1Core_LogEvent(n"ExampleMod", 2, "Optional data unavailable"); // Warning
```

Levels: 0 Debug, 1 Info, 2 Warning, 3 Error. Invalid levels, empty
messages, and strings longer than 2048 bytes are rejected. Logging is
best-effort and has no CET dependency. These logs are a starting point for
future crash-report attachments, not yet a crash handler, Advanced Crash
Reporter integration, or arbitrary console command execution.

## Bulk reset controls (post-v0.4.0 development)

The All Mods page has a two-click **Reset mod to defaults** action. Each named
group (including the default General section) has its own two-click **Reset
group** action. These controls only reset currently registered settings to
mod-declared defaults. Other mods and groups are left unchanged. The existing
Save button or F11 close persists the reset values using the unchanged
schema-v1 `settings.json` format. No profiles are removed or altered.

Mod authors can call `M1Core_ResetModSettings(modName)` or
`M1Core_ResetGroupSettings(modName, groupName)` and then
`M1Core_SaveSettings()`. Group metadata must have been registered first.
The settings search now matches group names as well as setting/mod names.

## Enum & keybind API (v0.3.0 development)

Existing Bool, Int32 and Float APIs remain unchanged. The new optional types use their own
registration/read/write functions and remain isolated by mod ID.

```redscript
// Values are zero-based indexes: Off=0, Stealth=1, Aggressive=2
M1Core_RegisterEnumSetting(n"ExampleMod", n"Mode", 1, "Off|Stealth|Aggressive");
let mode = M1Core_GetEnumSetting(n"ExampleMod", n"Mode");
M1Core_SetEnumSetting(n"ExampleMod", n"Mode", 2);

// Key codes are Windows VK values; 0 means "Unbound"; e.g. 117 is F6.
M1Core_RegisterKeybindSetting(n"ExampleMod", n"Action", 117);
let key = M1Core_GetKeybindSetting(n"ExampleMod", n"Action");
M1Core_SetKeybindSetting(n"ExampleMod", n"Action", 118);
```

- Enum option labels use a pipe-separated `String`, up to 64 unique nonempty choices.
- The selected enum **index** is persisted as a typed `enum`, not just a generic integer.
  Adding/removing options clamps an old saved index to the new range.
- Keybinds use Win32 virtual-key numbers from 0 to 255 and persist as typed `keybind`.
  **m1core stores the code; it does not execute the mod's action.** The consuming mod
  remains responsible for checking keyboard events when appropriate.
- The UI reserves **F11** for opening and closing m1core. Escape cancels an active
  *key-capture operation* only, and does not close the m1core overlay.
- The preview registers `Camera Mode` and `Quick Action` fields to validate
  the dropdown, key capture and two-phase settings persistence.
- The v0.3.0 C++ build and CTest cover these types. The user confirmed dropdowns and keybind persistence in-game on 2026-11-19; the newest bulk reset and diagnostic APIs still require an in-game smoke check.

## Optional in-game native API test

The **same single development ZIP** contains `r6/scripts/m1core/m1core_smoke.reds`.
The helper does nothing unless you explicitly call it; CET is only a test console,
not a runtime dependency of m1core.

1. Install the **one** game-ready ZIP into the game root and load a save.
2. In CET console run `print(Game.GetPlayer():M1Core_SmokeTest(1))`. Result **0** means the native API can register, get, set, reset, and save test settings.
3. Close the game, restart, and load a save. Run `print(Game.GetPlayer():M1Core_SmokeTest(2))`. Result **0** means values persisted across restart.
4. Delete `r6/scripts/m1core/m1core_smoke.reds` whenever you no longer need this optional test helper.

**Safe recovery:** delete `red4ext/plugins/m1core/overlay.enable` and restart to turn off UI/input hooks while keeping the core settings DLL installed.

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
2. **Milestone 2 (in-game validated):** opt-in D3D12/ImGui render path, guarded Win32 input, three exit mechanisms, game focus and resize handling. F11 toggle and mouse cursor were user-tested in game on 2026-11-19. The enum/keybind UI was confirmed in-game on 2026-11-19. F11 toggle, Close button, and emergency recovery must work; never leave game input blocked.
3. **Milestone 3 (partially implemented):** searchable sidebar, separate Keybinds/Profiles/About tabs, enum dropdowns, keybind capture, and named profiles. Named profiles now exist; finer per-mod grouping and final visual customization remain future work.
4. **Milestone 4:** live in-game validation, compatibility audit, test mod, release ZIP and API freeze.
5. **After m1core is stable:** migrate existing CP2077 mods individually, maintaining their working behavior.

No existing mod is changed by this milestone. Do not call m1core finished based on a successful build alone.
