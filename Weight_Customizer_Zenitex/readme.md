# Weight Customizer — Zenitex Compatibility

Optional compatibility add-on for **Weight Customizer** and **Zenitex Modular Military Backpack (ArchiveXL)**.

**Weight Customizer is required. This add-on is not a standalone replacement.**

## Required mods

1. [Weight Customizer](https://www.nexusmods.com/cyberpunk2077/mods/39409) **1.0.0** (includes its own M1 Arc, RED4ext and redscript requirements).
2. [Zenitex Modular Military Backpack — ArchiveXL](https://www.nexusmods.com/cyberpunk2077/mods/18152) **1.5**.
3. [Equipment-EX](https://github.com/psiberx/cp2077-equipment-ex) to wear the backpack through its cosmetic Back slot (plus its own ArchiveXL/TweakXL/Codeware requirements).

The optional patch's release dependency metadata intentionally lists **Weight Customizer** as its direct dependency; the other mods supply the backpack and its equipment slot.

## What it does

- Detects an **equipped** Zenitex backpack in Equipment-EX's `OutfitSlots.Back` slot. Merely carrying one in inventory does not count.
- Restores the original rarity-based carry-capacity bonuses for appearance-only Equipment-EX backpacks: Common +200 kg, Uncommon +210 kg, Rare +220 kg, Epic +230 kg, Legendary +240 kg, Iconic +250 kg.
- Adjusts the same 0.5-second Weight Customizer stat cycle so the backpack bonus is **added on top of** the M1 Arc limit, instead of being immediately canceled.
- Also grants the backpack bonus when **Custom carry capacity** is disabled; the configured Weight Customizer total is then not enforced.
- Removes only this add-on's owned temporary modifier when the backpack is unequipped.
- For a backpack equipped in the **vanilla Outer Chest/Torso slot**, counts its original Zenitex bonus **on top of** Weight Customizer's configured total without adding a second stat modifier. With Weight Customizer disabled, the vanilla bonus is left to Zenitex's intrinsic modifier (so missing intrinsic bonuses in 2.5 still require an in-game check).
- Does not redistribute Zenitex assets, add CET functionality, or change Weight Customizer's M1 Arc UI.

## Installation

1. Install **Weight Customizer** normally first.
2. Install the original **Zenitex backpack** and **Equipment-EX**.
3. Extract the **Weight Customizer — Zenitex Compatibility** game-root ZIP **last** to the Cyberpunk 2077 directory. **Allow it to replace** `r6/scripts/WeightCustomizer/WeightCustomizer.reds` from Weight Customizer.

The patch replaces that **one REDscript file only**; it leaves Weight Customizer's M1 Arc manifest, settings, and translations installed and unchanged.

## Uninstall and updates

**Reinstall Weight Customizer** after removing this patch. Simply deleting the replacement REDscript file would remove the core mod's script too. Likewise, after updating Weight Customizer, reinstall a matching version of the compatibility patch last.

This is a version-specific replacement of Weight Customizer's 1.0.0 REDscript implementation. Do not combine it with other patches that replace the same file.

## Source and testing status

Implemented against Zenitex's own v1.5 YAML records and Equipment-EX's published Back-slot preview-item behavior. Offline checks and packaging do not prove game REDscript compilation or runtime behavior. **In-game verification is still required before release.**

Suggested test: with Weight Customizer set to 500 kg, equip a Legendary Zenitex backpack in Equipment-EX and verify 740 kg; unequip and verify 500 kg. Repeat after saving/loading, with an Iconic backpack, with the core setting disabled, and with a vanilla-slot backpack to rule out double stacking.
