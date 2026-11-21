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
\`registerForEvent\`, \`registerHotkey\` and ImGui APIs:

| Extension | Meaning |
| --- | --- |
| \`.m1s\` | m1core-readable Lua source (new default) |
| \`.m1c\` | LuaJIT binary bytecode (optional, LuaJIT version-specific) |
| \`.lua\` | Existing Lua source compatibility |

Put mods under \`red4ext/plugins/m1core/mods/<mod-id>/\`. A mod must have
one of \`init.m1s\`, \`init.lua\`, \`init.m1c\`; **source wins** in that order.
Normal \`loadfile()\`, \`dofile()\`, and sandbox \`require()\` resolve
extensionless scripts via \`.m1s\`, \`.lua\`, then \`.m1c\`, and modules may
also use \`folder/init.m1s\`, \`folder/init.lua\` or \`folder/init.m1c\`.
Explicit filenames such as \`loadfile("module.m1c")\` are supported.

\`.m1s\` does not define a new language: it is ordinary LuaJIT-compatible
Lua source. \`.m1c\` must contain **actual LuaJIT bytecode**, not renamed
text or standard Lua bytecode; the embedded VM rejects incompatible
versions. Source modes reject embedded binary bytecode, and
\`loadstring()\` still refuses bytecode. Do not accept bytecode from
untrusted sources: LuaJIT bytecode may bypass sandbox assumptions and
it is **not encryption or intellectual-property protection**.

Example \`init.m1s\`:

\`\`\`lua
registerForEvent("onInit", function()
    print("m1core script started")
end)
\`\`\`

The scripting loader and its extension tests compile under Windows CI;
real in-game extension loading is a separate verification step.
