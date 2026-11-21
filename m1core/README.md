# m1core

**Project handoff / persistent development status:** [`PROJECT_STATUS.md`](PROJECT_STATUS.md). This is the canonical link to use when resuming work in a new chat; update it alongside code and CI changes.

**Native Cyberpunk 2077 mod settings framework — development build**

m1core is a standalone Windows x64 DLL loaded by **RED4ext**. It is being built directly on the MIT-only Cyber Engine Tweaks 1.38.1 source archive supplied by yamashi for this project. It does **not** require a separate Cyber Engine Tweaks installation.

The native game-window, DirectX 12, keyboard/mouse, input capture, overlay lifecycle and embedded scripting components are derived from that connected source, not independently re-created substitutes. The original MIT notices and third-party notices remain with the project. CET's proprietary/restricted fonts and icons and its ASI loader are not included.

## Development status

The author-supplied runtime **loads and responds to mouse and keyboard in Cyberpunk 2077 2.5**, confirmed in-game on November 21, 2026. The user-supplied Dear ImGui theme renders correctly. A subsequent build adds the **m1core application interface** with Settings, Keybinds, Profiles, Logs and About, and ports the earlier native REDscript mod-settings registry onto the same runtime. The user confirmed the native m1core interface and Profiles UI work in-game; the cloud-sync loop was also fixed and user-confirmed. The subsequent `.m1s` / `.m1c` loader build passed Windows CI, but **real mod script execution is not yet confirmed in-game**. Passing CI is not proof of all in-game behavior.

The project is unreleased. Support for fullscreen, windowed and borderless display modes, Alt+Tab, closure, and compatibility with other mods must remain reliable before release.

## Lua scripting extensions

m1core loads `init.m1s` as readable Lua source, `init.m1c` as LuaJIT bytecode, and traditional `init.lua` for compatibility. Scripts live under `red4ext/plugins/m1core/mods/<mod-id>/`. `.m1s` is the preferred source extension, `.m1c` is optional, and the loader supports module imports through the same formats.

A matching LuaJIT compiler can produce bytecode using `luajit -b init.m1s init.m1c`; the m1core runtime requires compatible LuaJIT bytecode, not ordinary Lua bytecode. Only install bytecode from trusted sources. See [DEVELOPERS.md](DEVELOPERS.md) for module-loading details and safety limits.

## Source layout

- [`core/`](core/) — active CET-derived runtime, RED4ext DLL entry point, Dear ImGui UI, native registry, keybindings, game hooks and scripting.
- [`native/`](native/) — preserved earlier independent native implementation, including its registry/state classes and supplied `ImGuiCustomTheme.h`. This is **not** the renderer used by the new DLL.
- [`DEVELOPERS.md`](DEVELOPERS.md) — native JSON settings API, schema, persistence and profiles.
- [`core/BUILD.md`](core/BUILD.md) — Windows build system and source attribution.

The active build uses the CET-derived project's xmake dependency graph and produces `m1core.dll`; no `.asi` loader is shipped. The ZIP is installed at the **game root**, with `red4ext/plugins/m1core/m1core.dll` inside. The original donated-source snapshot is retained in Git history.

## Interface

m1core uses standard **Dear ImGui** windows, tabs, tables, checkboxes, sliders, dropdowns and buttons. Its appearance comes from the user-provided ImGui style, with no custom-rendered controls and no bundled CET icon or font assets. The donor engine still owns cursor, keyboard and mouse capture; no duplicate input backend is installed by the settings registry.

Registered mods appear in the Settings tab. Each can expose boolean, integer, float and choice settings, grouped by name. The Profiles tab stores/restores native settings. The Keybinds tab delegates to the runtime's original physical keyboard binding engine. CET-only developer panels are no longer part of the main m1core UI.

## Licensing and credits

m1core's own code is MIT. The CET-derived runtime uses the special **MIT-only source package provided directly by yamashi**; preserve `core/LICENSE-MIT` and third-party notices. Do not copy third-party fonts, icons, unauthorized binaries, or the ASI loader. The m1core developer will personally handle Nexus Mods release credits when the mod is published.

**Branch:** `dev/m1core-cet-foundation`. No main-branch merge until the runtime, settings and input lifecycle are validated in game. The old `dev/m1core` implementation remains intact while the new foundation is tested.

CI #44919804951 proved actual LuaJIT source/bytecode execution with the shared resolver. The m1core runtime source was not changed by this test-harness work.
