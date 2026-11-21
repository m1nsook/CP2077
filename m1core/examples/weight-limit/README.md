# Weight Limit — m1core consumer / script format integration example

This is a **functional sample mod** to verify m1core's new Lua settings API and
`.m1s` / genuine LuaJIT `.m1c` entrypoints in Cyberpunk 2077 2.5. It is
**not release/game verified yet**.

## Install (choose one)

Download **one** GitHub Actions artifact, either
`m1core-weight-limit-source` or `m1core-weight-limit-bytecode`, and extract
directly into your **Cyberpunk 2077 game root**. Both contain exactly one
entrypoint in `red4ext/plugins/m1core/mods/weight-limit-sample/`:
- Source: `init.m1s`
- Compiled: `init.m1c` — real bytecode from m1core's pinned LuaJIT build.

**Do not install both**: `init.m1s` intentionally takes priority over
`init.m1c`. Install the current `m1core.dll` from the framework's separate
`m1core-native-x64-dev` artifact first. RED4ext is required; **CET is not**.

## Using the mod

Open the m1core overlay (your existing configured shortcut), select
**Settings > Weight Limit (Sample)**. Toggle *Custom weight limit* and adjust
*Carry capacity (kg)* (50–5000 kg, default 500). The script polls settings
every 0.5 seconds, then sets the player's current total carry capacity using
a **non-saved additive modifier**. Disabling or unloading the Lua mod removes
the modifier it created. It does **not** use `RemoveAllModifiers`,
`AddSavedModifier`, or mutate other mods' game stats. The setting itself is
saved by m1core in `red4ext/plugins/m1core/settings.json`.

Checks for a new sample build: install **only** the source package and confirm
the Settings entry and actual in-game carry capacity. Remove it, install
**only** the bytecode package, and check again. Reports of errors can include
`red4ext/plugins/m1core/scripting.log` and the mod's log.

The CI test compiles real `.m1c`, executes both formats on the same embedded
LuaJIT with the real m1core registry bridge and a *mocked* game StatsSystem.
A green build cannot prove Cyberpunk's actual stats RTTI bridge or
`ScriptContext` lifecycle; that needs one in-game confirmation.

## How to edit

Modify `init.m1s` here and let the Windows `m1core-core.yml` workflow
compile `init.m1c`. The bytecode is LuaJIT-version-specific; it is not
encrypted and doesn't replace your MIT-licensed source.

The API functions used are
`m1core.registerJson(modId, displayName, schemaJson)` and
`m1core.getJson(modId, settingId)`; script entry points use existing
`registerForEvent` callbacks.
