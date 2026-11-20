# M1CORE — rebuilt on Cyber Engine Tweaks

M1CORE is a new MIT-licensed settings framework loaded by **Cyber Engine
Tweaks (CET)**. CET itself owns keyboard shortcuts, mouse and cursor,
window focus, DirectX presentation and the Dear ImGui frame lifecycle.
M1CORE no longer ships an independent RED4ext input/rendering plugin.

The deleted native implementation remains available in Git history on
\`archive/m1core-before-cet-reset-20261120\`. It is **not** reused by
this implementation.

## Installation

1. Install a compatible Cyber Engine Tweaks version normally.
2. Extract the GitHub Actions artifact to the Cyberpunk 2077 **game root**.
   Its archive contains exactly one root directory: \`bin/\`, holding
   \`bin/x64/plugins/cyber_engine_tweaks/mods/m1core/\`.
3. Remove the **old m1core files** at
   \`red4ext/plugins/m1core/\` and \`r6/scripts/m1core/\`. The new CET
   mod must not be mixed with the native version.
4. Open the **CET overlay**. M1CORE is available there; for an additional
   show/hide shortcut, use CET's **Bindings** screen and set
   \`M1CORE - Show / hide settings\` to any key. No default F-key.

No Docker, custom DLL, raw mouse handler or custom cursor is included.

## Settings API for CET mods

CET Lua mods can call \`GetMod("m1core")\` from their \`onInit\` callback,
after CET has loaded the mod contexts:

\`\`\`lua
registerForEvent("onInit", function()
  local core = GetMod("m1core")
  if not core then return end

  core.register("my.mod", "My Mod", {
    {id="enabled",type="boolean",default=true,group="General"},
    {id="weight",type="integer",min=0,max=500,default=100,
     group="Gameplay",label="Carry Weight"},
    {id="multiplier",type="float",min=0.5,max=3,default=1},
    {id="mode",type="choice",choices={"normal","hard"},default="normal"}
  })
end)
\`\`\`

Use \`core.get("my.mod", "enabled")\` to read live values, and
\`core.revision()\` to detect changes. The API also provides \`set\`,
\`reset\`, \`save\`, \`saveProfile\`, \`loadProfile\`, \`deleteProfile\`,
\`profiles\`, \`show\`, and \`hide\`. Values and profiles are saved
inside CET's sandboxed \`m1core/settings.json\`.

CET already manages hotkeys and key rebinding. Individual CET mods
should call CET's \`registerHotkey\` instead of asking M1CORE to
replace CET's binding system.

## Compatibility boundary

**Breaking change:** the old native REDscript functions named
\`M1Core_*\` are **not** provided by this CET Lua replacement.
Mods depending on those REDscript natives will need to migrate or use a
separate compatibility bridge. The new code must not be represented as
a drop-in replacement for those callers. Do not keep the deleted
RED4ext DLL installed.

## License

M1CORE source is **MIT**. CET is **GPLv3 with LICENSE-EXCEPTION**.
The exception's section 2 expressly permits third-party Lua mods using
CET's public scripting/UI/storage APIs to use their own licenses.
No GPLv3 CET core source was copied into M1CORE. Other mods' MIT
licenses are unchanged.

## Verification

The workflow checks Lua syntax, registry persistence and profile tests,
and packages a single game-ready ZIP. Automated checks do not prove
Cyberpunk in-game UI appearance, nor compatibility with old REDscript
mods. No in-game signoff is claimed.
