# m1core — native DLL with CET-hosted settings UI

**Development:** v1.0.0-dev.2; unreleased. Clean restart from the earlier
unreleased m1core experiments. The old native overlay, rendering hooks,
Win32 cursor hooks, RAWINPUT and redscript sources are deleted from this
branch; no published mods require compatibility with those experiments.

## Architecture

M1CORE is **a native RED4ext DLL**. The main module is
\`red4ext/plugins/m1core/m1core.dll\`. It owns:
- A thread-safe, versioned JSON settings-state store.
- Persistent values and named profile snapshots.
- Atomic settings writes and unreadable-file protection.
- Native REDengine RTTI functions for reading/saving settings.

A small companion CET Lua mod in
\`bin/x64/plugins/cyber_engine_tweaks/mods/m1core/\` provides:
- The setting descriptors and live mod API through \`GetMod("m1core")\`.
- The ImGui settings interface.
- Menu show/hide via **CET's own user-configured binding**.

This uses CET's real overlay, real mouse cursor, focus/input handling,
ImGui frame and hotkey system. **There is no second DirectX renderer,
Win32 hook, virtual cursor or input architecture in m1core.**

### Native ↔ CET bridge

The RED4ext DLL registers these native game functions, which CET Lua
calls through its existing \`Game\` RTTI interface:

| Function | Role |
|---|---|
| \`Game.M1Core_NativeVersion()\` | DLL version |
| \`Game.M1Core_NativeReady()\` | Native storage readiness |
| \`Game.M1Core_NativeGetState()\` | Read authoritative JSON state |
| \`Game.M1Core_NativeSetState(json)\` | Validate and persist JSON state |
| \`Game.M1Core_NativeRevision()\` | Native revision counter |
| \`Game.M1Core_NativeLastError()\` | Storage error details |

The Lua frontend does **not** quietly fall back to a separate settings
file if the DLL is missing. Both parts must be installed.

## Installation

Requires up-to-date **Cyber Engine Tweaks and RED4ext**.

1. Remove any files left over from the **old unreleased m1core prototype**
   (\`red4ext/plugins/m1core/\` and \`r6/scripts/m1core/\`), if present.
2. Extract the **single GitHub Actions ZIP** to the Cyberpunk 2077 game
   root. It has two root directories: \`red4ext/\` and \`bin/\`.
3. In CET's **Bindings**, set
   \`M1CORE - Show / hide settings\` to a key of your choice.
   No key is assigned by m1core, and none is hard-coded.
4. Open CET's overlay to access the settings UI.

Stored settings live alongside the native DLL:
\`red4ext/plugins/m1core/settings.json\`.

## Public settings API for CET mods

\`\`\`lua
registerForEvent("onInit", function()
  local settings = GetMod("m1core")
  if settings then
    settings.register("my.example", "My Example", {
      {id="enabled", type="boolean", default=true, group="General"},
      {id="capacity", type="integer", min=1, max=500, default=100,
       label="Carry capacity", group="Gameplay"},
      {id="difficulty", type="choice",
       choices={"normal", "hard"}, default="normal"}
    })
  end
end)

-- Live read from another CET mod
local settings = GetMod("m1core")
local enabled = settings and settings.get("my.example", "enabled")
\`\`\`

The new version was never released, so no migration layer for the
original prototype is necessary.

## License and dependencies

m1core's code is **MIT**. CET's own implementation is GPLv3 with
\`LICENSE-EXCEPTION\`, permitting separately licensed mods that use its
public scripting APIs. We use CET's public Lua/ImGui/RTTI interface, not
copied CET source. RED4ext SDK is separately MIT. Other mods and
their licenses are unchanged.

## Verification limits

CI builds an actual **Windows x64 m1core.dll**, tests native JSON storage,
runs Lua tests, and checks the final combined ZIP. This does not prove
runtime behavior or visual results inside Cyberpunk. In-game verification
is still required before any public release.
