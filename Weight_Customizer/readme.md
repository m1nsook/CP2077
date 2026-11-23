# Weight Customizer

Set V's total carry capacity from **50 to 5000 kg** in the
[M1 Arc](https://github.com/m1nsook/m1arc) settings panel.

Changes apply in-game every 0.5 seconds. Disabling the setting removes
only Weight Customizer's temporary stat modifier, leaving perks, cyberware
and other mods' modifiers alone.

## Requirements

- Cyberpunk 2077 **2.50**
- RED4ext
- redscript
- **M1 Arc 1.0.0 or newer**

## Installation

Extract the game-root ZIP into your Cyberpunk 2077 directory. Install M1 Arc
separately. Weight Customizer appears under the **Gameplay** scope.

Open the mod in M1 Arc, enable **Custom carry capacity**, and set **Carry
capacity (kg)**. The default is **500 kg**. M1 Arc stores your settings and
profiles centrally.

If you installed the earlier **Carry Weight** development build, remove its
old `r6/scripts/M1ArcCarryWeight/` directory and
`red4ext/plugins/m1arc/mods/m1nsook.weight/` folder first, so both mods do
not change the same stat.

## Translations

Includes English, Polish, and Korean JSON labels. M1 Arc loads the translation
matching your selected framework language. All stored setting IDs stay in
English, so switching languages preserves your values.

## Status

Development build. CI checks the manifest, translation structure, REDscript
API references, and single game-root archive. Real REDscript compilation
against game scripts and in-game modifier behavior still need verification.
