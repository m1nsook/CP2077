# m1core

**Independent native modding framework for Cyberpunk 2077**

m1core is a standalone **RED4ext x64 plugin** providing a Dear ImGui
settings interface, keybinds, profiles, persistent mod settings, and an embedded
LuaJIT runtime. It installs as `red4ext/plugins/m1core/m1core.dll` and **does
not require a separate Cyber Engine Tweaks installation**.

Project license: **MIT — Copyright (c) 2026 민숙 MISO @m1nsook** ([LICENSE](LICENSE)).
m1core is its own framework with its own name, API, design, fonts and icons.
Its internal rendering and input implementation includes code donated with
permission from the original author; there is no runtime dependence on the
original project. Copyright/license notices for independent third-party
libraries and font assets remain in the distributed package.

## Verified functionality

- Loads as a RED4ext plugin in Cyberpunk 2077 2.5.
- Native Dear ImGui UI supports Settings, Keybinds, Profiles, Logs and About.
- Mouse/keyboard/game input, the startup input lifecycle and Profiles UI
  were confirmed in-game.
- The **Weight Limit** sample mod registers its own settings and works in-game.
- **Karla Regular, Medium, SemiBold and Bold** provide a readable UI hierarchy, **JetBrains Mono** is the logs font and
  **Phosphor Icons** appear throughout the interface. Their appearance was
  confirmed in-game.
- `.m1s` readable Lua source and `.m1c` actual LuaJIT bytecode are both
  checked in native Windows CI, although the user has not identified
  which one was installed during the gameplay sample check.

**Settings save automatically:** changing a checkbox, slider or choice commits
to `red4ext/plugins/m1core/settings.json` immediately; you do **not** need a
Save button. If a file cannot be opened or updated, the Settings interface now
displays a visible warning. Automatic save/restart behavior is checked by
Windows native regression tests; the current in-game restart report is still
awaiting confirmation with the latest build.

The framework is still in development. Test status and open items are maintained
in [PROJECT_STATUS.md](PROJECT_STATUS.md).

## Developer interface

Other mods may register settings that appear directly in m1core, and read or
change values using its native RTTI/REDscript API or Lua `m1core` API.
The registry supports checkboxes, sliders, choices, profiles and persistence.
See [DEVELOPERS.md](DEVELOPERS.md).

Place Lua mods under `red4ext/plugins/m1core/mods/<mod-id>/`.
Entrypoints: `init.m1s` (readable source), `init.m1c` (LuaJIT bytecode), or
`init.lua` (compatible Lua source). Install only one entrypoint variant
per mod. See [examples/weight-limit](examples/weight-limit/README.md).

## Building

Native project sources and Windows xmake build instructions:
[core/BUILD.md](core/BUILD.md). The GitHub Actions workflow produces
a single **game-root** framework package, including the DLL, Karla,
JetBrains Mono, Phosphor icons and independent third-party license notices.

The repository also retains archived early native-renderer research in
`native/`; the active native plugin implementation lives in `core/`.

## Release credits

The maintainer will give the original technical contributor a shout-out in
the **Nexus Mods release description**, as personally requested.
This is a release acknowledgment, not a dependency or branding requirement.
