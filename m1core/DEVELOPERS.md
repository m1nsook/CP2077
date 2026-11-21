# m1core — developer API (unreleased)

m1core registers native functions through RED4ext's RTTI system. These APIs are for mods wishing to expose **settings**, not a dependency on the separate Cyber Engine Tweaks runtime. The native registry stores each mod's values in the m1core plugin directory and renders them in the Settings tab.

## Available native functions

| Function | Parameters | Result |
| --- | --- | --- |
| `M1Core_Version` | none | String |
| `M1Core_RegisterJson` | `modId: String, displayName: String, schemaJson: String` | Bool |
| `M1Core_GetJson` | `modId: String, settingId: String` | String (JSON, or `null`) |
| `M1Core_SetJson` | `modId: String, settingId: String, payload: String` | Bool |
| `M1Core_ResetSetting` | `modId: String, settingId: String` | Bool |
| `M1Core_ResetMod` | `modId: String` | Bool |
| `M1Core_SaveProfile` | `name: String` | Bool |
| `M1Core_LoadProfile` | `name: String` | Bool |
| `M1Core_DeleteProfile` | `name: String` | Bool |

These are native RTTI functions registered by `core/src/m1core_registry_api.cpp`, backed by `native/registry.cpp` and `native/state.cpp`. The registration calls must occur **after m1core and its RTTI callbacks are initialized**. A separately packaged Lua frontend or CET installation is not necessary to use this native settings registry.

## Schema

Pass a JSON array to `M1Core_RegisterJson`. A small example:

```json
[
  {"id":"enabled","kind":"boolean","label":"Enabled","default":true,"group":"General"},
  {"id":"level","kind":"integer","label":"Level","default":3,"min":0,"max":10,"step":1,"group":"Gameplay"},
  {"id":"opacity","kind":"float","label":"Opacity","default":0.85,"min":0.1,"max":1.0,"step":0.05,"group":"Interface"},
  {"id":"mode","kind":"choice","label":"Mode","default":"normal","choices":["normal","quiet","aggressive"],"group":"Gameplay"}
]
```

A `modId` and each `id` must contain 1–96 characters from `A-Z`, `a-z`, `0-9`, `_`, `-` or `.`. A registration accepts 1–250 unique settings. Required fields for each setting are `id`, `kind` and `default`. Optional labels/groups/descriptions are supported. Integer and float values are clamped to `min`/`max`; choice values must match one of the declared strings. Duplicate IDs, incorrect value types, invalid JSON and malformed schemas are rejected.

`M1Core_SetJson` accepts a JSON-formatted value string: `"true"` for a boolean, `"5"` for an integer, `"0.8"` for a float, or `"\"quiet\""` for a choice. `M1Core_GetJson` returns the serialized value. Settings changes are committed transactionally to `red4ext/plugins/m1core/settings.json`, and persist after restart; saved profile names and values share that file.

Profiles contain the saved values for **all mods**, including mods that are currently unloaded. Loading a profile merges its values into `settings.json` without deleting settings created after the profile snapshot; when an unloaded mod registers again, it reads the restored values. For currently registered mods, profile values are type-checked against their schemas before committing. Invalid profiles fail without partial updates. Profile names follow the same identifier rules as mod and setting IDs.

The native settings store is **separate** from the runtime's own `config.json`, `bindings.json`, and `persistent.json`. Do not replace or delete those files to configure another mod.

## Development / compatibility

The CET-derived input, graphics and hotkey runtime is intentionally separate from the native JSON registry. Mod settings must never hook or capture Windows input themselves. The project is not released; function registration and gameplay interaction still require validation with a consuming mod before other developers rely on this ABI. The existing internal hotkey persistence key keeps its historical value for backward compatibility even though the visible UI is called m1core.


## Script file extensions

m1core supports three Lua script extensions while retaining CET-style
`registerForEvent`, `registerHotkey` and ImGui APIs:

| Extension | Meaning |
| --- | --- |
| `.m1s` | m1core-readable Lua source (new default) |
| `.m1c` | LuaJIT binary bytecode (optional, LuaJIT version-specific) |
| `.lua` | Existing Lua source compatibility |

Put mods under `red4ext/plugins/m1core/mods/<mod-id>/`. A mod must have
one of `init.m1s`, `init.lua`, `init.m1c`; **source wins** in that order.
Normal `loadfile()`, `dofile()`, and sandbox `require()` resolve
extensionless scripts via `.m1s`, `.lua`, then `.m1c`, and modules may
also use `folder/init.m1s`, `folder/init.lua` or `folder/init.m1c`.
Explicit filenames such as `loadfile("module.m1c")` are supported.

`.m1s` does not define a new language: it is ordinary LuaJIT-compatible
Lua source. `.m1c` must contain **actual LuaJIT bytecode**, not renamed
text or standard Lua bytecode; the embedded VM rejects incompatible
versions. Source modes reject embedded binary bytecode, and
`loadstring()` still refuses bytecode. Do not accept bytecode from
untrusted sources: LuaJIT bytecode may bypass sandbox assumptions and
it is **not encryption or intellectual-property protection**.

Example `init.m1s`:

```lua
registerForEvent("onInit", function()
    print("m1core script started")
end)
```

Windows [CI #44919804951](https://github.com/m1nsook/CP2077/actions/runs/44919804951) passes the compiled DLL, registry/state, extension-resolution, and **actual LuaJIT execution** tests. `m1core_script_runtime_tests` executes `.m1s` and `.lua`, generates a real LuaJIT `.m1c` using `lua_dump`, executes it in binary mode and rejects invalid formats. **In-game execution through m1core's `LuaSandbox`/`ScriptContext` has not yet been verified.**

To compile an optional `.m1c` from source using a **matching LuaJIT version**:

```powershell
luajit -b init.m1s init.m1c
```

Prefer shipping the readable `.m1s` file where possible. Bytecode is not encryption and can be incompatible between LuaJIT builds.

### Native Lua settings interface (first consuming sample)

In addition to the REDscript/RTTI `M1Core_*` functions, m1core exposes a
**native Lua table**, `m1core`, to sandboxed scripts. This is registered
before `init.m1s` / `init.lua` / `init.m1c` is executed and uses the same
data-only registry and `settings.json` as the built-in Settings tab:

- `m1core.registerJson(modId, displayName, schemaJson)` returns a boolean.
- `m1core.getJson(modId, settingId)` returns serialized JSON or `"null"`.
- `m1core.setJson(modId, settingId, valueJson)` returns a boolean.
- `m1core.resetSetting(modId, settingId)` returns a boolean.

Read source and installable variants of the first actual mod consumer at
[`m1core/examples/weight-limit`](examples/weight-limit/README.md).
This example registers an enable toggle and a total carry-capacity slider;
reads values while the game runs; applies a non-saved additive
`CarryCapacity` stat modifier to the player; and removes only its own
modifier on disable/shutdown. It requires **m1core and RED4ext only** (not a
separate CET installation). The source and compiled entrypoints are alternate
installs: **never place `init.m1s` and `init.m1c` together** if you intend
to verify both code paths.

Windows CI builds an integration test using the **actual native settings
registry** and **actual pinned LuaJIT** for both scripts. Game stats, game
initialization and the Cyberpunk `ScriptContext` lifecycle are not exercised
by this mock-based CI test, and still require a real game check.

### LuaJIT integration fixture

The standalone developer fixture is at
[`m1core/examples/lua-smoke/`](examples/lua-smoke/).
It tests `init.m1s` + `require('helper')` from `helper.m1s`, plus `compat.lua`.
The native CI target `m1core_script_runtime_tests` compiles and executes
real LuaJIT source, generates genuine LuaJIT bytecode via `lua_dump`,
loads it in binary-only mode and rejects text/binary mode mismatches.
This does not yet verify the in-game `LuaSandbox` lifecycle.
