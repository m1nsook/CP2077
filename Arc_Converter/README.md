# Arc Converter - Automatic Item Processing

Cyberpunk 2077 2.50 mod. Requires RED4ext, redscript and M1 Arc 1.0.0. No CET.

## What it does

- **Auto disassembly:** Watches *new* additions to V's inventory and disassembles qualifying weapons at selected tiers. Never scans or disassembles weapons already in V's backpack when installed/enabled.
- **Auto ammunition crafting:** When the equipped weapon's reserve ammunition is below a configurable percentage, crafts one batch using the vanilla ammunition recipe and the player's real components. Rechecks resources and ammo capacity before further crafting.

The two features can be enabled separately from M1 Arc. Disassembly and every tier selector default to **off**, so nothing is destroyed on installation. Ammo crafting defaults to **on**.

## Item safety

Only newly acquired, non-silent weapon additions outside game menus can be processed. Before requesting any disassembly, the mod checks the item again and refuses equipped, iconic, quest, unequip-blocked, long-lasting or crafted weapons. The game's own `CraftingSystem.CanItemBeDisassembled` also has to accept the item. Disassembly is queued through the **normal game crafting system**, not by deleting the item and granting arbitrary components.

Note: The inventory callback reports *an addition*, not specifically whether the source was loot. Acquisitions during menus and silent internal additions are ignored, but other newly acquired eligible weapons outside menus can qualify. This should be tested in game before release.

## Ammunition

The first version supports the game's four vanilla ammunition types: handgun, rifle, shotgun and sniper. Only the actively used weapon's ammunition is replenished. Vanilla recipe batch amounts, costs and crafting effects are used, and processing stops when components are insufficient. The mod never grants free bullets. If the player lacks components, the normal ammunition shortage remains.

## Installation

Copy the `r6/` and `red4ext/` folders into the Cyberpunk 2077 installation root. Configure **Arc Converter - Automatic Item Processing** inside M1 Arc.

## Development

This is the first development build, **not yet verified in Cyberpunk 2077 2.50**. Check with a save backup:

1. Launch a save with old low-tier weapons and verify they remain untouched.
2. Enable auto-disassembly and one tier, loot a non-iconic weapon of that tier; verify only that weapon is disassembled and components are awarded.
3. Loot an iconic, quest, equipped, or crafted weapon and verify it is kept.
4. Fire a supported weapon to reach the ammo threshold; verify one vanilla batch is crafted, components decrease, and ammo limits are honored.
5. Reload, change saves, toggle settings, open shops/crafting menus, and switch weapons while testing for no unexpected disassembly.
