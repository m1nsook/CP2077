# Carry Weight — native M1 Arc test mod

Carry Weight is a **real, live REDscript weight-limit mod** for testing the
[M1 Arc](https://github.com/m1nsook/m1arc) mod-manifest integration. It
does **not** require Cyber Engine Tweaks, Mod Settings, or Lua.

## Requirements

- Cyberpunk 2077 2.5
- RED4ext and REDscript
- **M1 Arc 0.1.0 or newer**, with the native manifest loader. A previous
  M1 Arc build that lacks \`mods/\` discovery will not register the settings.

## Installation

Extract the standalone ZIP **once** into the Cyberpunk 2077 game root.
Install M1 Arc separately first. The ZIP contains:

\`\`\`text
red4ext/plugins/m1arc/mods/m1nsook.weight/
├── mod.json
├── settings.json
└── language/
    ├── en_US.json
    └── en_US.json.example
r6/scripts/M1ArcCarryWeight/M1ArcCarryWeight.reds
\`\`\`

Open the M1 Arc overlay. **Carry Weight** appears automatically; no
\`M1Arc_RegisterJson\` call is needed in REDscript. Toggle *Custom carry
capacity* and adjust the limit from **50 to 5000 kg**, default **500 kg**.

The REDscript game system checks the setting every 0.5 seconds. It changes
V's actual carry capacity using a **temporary, non-saved additive stat
modifier**. Toggling OFF removes only this mod's own modifier, rather than
using \`RemoveAllModifiers\` or resetting other mods' stats. A player/save
change is handled without taking ownership of the old entity's modifiers.

M1 Arc persists the choices in the central
\`red4ext/plugins/m1arc/settings.json\`; do not put saved values inside the
mod's own \`settings.json\` schema file.

The language files are **templates** for the planned localization API;
the current manifest loader does not consume localized strings yet. The
English labels in the \`settings.json\` schema are authoritative for now.

**Status:** development. GitHub CI validates the manifest, packaging and
the REDscript consumer API references; a passing CI run does **not** mean
the REDscript compiled against game assets or worked in-game. A single
in-game check of carry capacity/disable/save reload is still required.
