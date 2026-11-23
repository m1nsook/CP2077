# Arc Converter - Automatic Item Processing

Cyberpunk 2077 2.50 mod. Requires RED4ext, redscript and M1 Arc 1.0.0. No CET.

## What it does

- **Auto disassembly:** Watches *new* additions to V's inventory and disassembles qualifying weapons at selected tiers. Never scans or disassembles weapons already in V's backpack when installed/enabled.
- **Auto junk selling:** Optionally convert junk and jewelry into eddies automatically at the game's regular selling prices, anywhere (no vendor needed). When enabled, this includes eligible junk already in the backpack, not just new pickups. Disabled by default. Quest, iconic, protected and non-junk items are skipped.
- **Auto ammunition crafting:** When the equipped weapon's reserve ammunition is below a configurable percentage, crafts one batch using the vanilla ammunition recipe and the player's real components. Rechecks resources and ammo capacity before further crafting.

All three features can be enabled separately from M1 Arc. Disassembly and every tier selector default to **off**, so nothing is destroyed on installation. Ammo crafting defaults to **on**. Automatic junk selling defaults to **off**.

## Item safety

Only newly acquired, non-silent weapon additions outside game menus can be processed. Before requesting any disassembly, the mod checks the item again and refuses equipped, iconic, quest, unequip-blocked, long-lasting or crafted weapons. The game's own `CraftingSystem.CanItemBeDisassembled` also has to accept the item. Disassembly is queued through the **normal game crafting system**, not by deleting the item and granting arbitrary components.

Note: The inventory callback reports *an addition*, not specifically whether the source was loot. Acquisitions during menus and silent internal additions are ignored, but other newly acquired eligible weapons outside menus can qualify. This should be tested in game before release.

## Ammunition

The first version supports the game's four vanilla ammunition types: handgun, rifle, shotgun and sniper. Only the actively used weapon's ammunition is replenished. Vanilla recipe batch amounts, costs and crafting effects are used, and processing stops when components are insufficient. The mod never grants free bullets. If the player lacks components, the normal ammunition shortage remains.

## Junk to eddies

The game classifies both `Gen_Junk` and `Gen_Jewellery` as junk in its own Sell Junk action. When **Automatically sell junk for eddies** is enabled, the separate junk processor scans V's inventory outside menus and gradually exchanges eligible stacks for eddies. It uses `RPGManager.CalculateSellPrice` rather than an invented price or vendor buyback.

- Selling is a **vendor-free exchange**: removed junk is credited as `Items.money`. No buyback stock or vendor wallet is involved.
- Junk that was **already in the backpack** is also eligible when this setting is enabled. **Unlike weapon disassembly**, the junk feature is not new-pickups-only.
- Quest, iconic, locked and other protected items are never processed; items with no positive selling price remain in the backpack.
- The conversion only pays after successful removal; on a failed currency grant it attempts to restore the removed junk.
- A processing delay guards save/player initialization, and item processing pauses while game menus are open.

## Installation

Copy the `r6/` and `red4ext/` folders into the Cyberpunk 2077 installation root. Configure **Arc Converter - Automatic Item Processing** inside M1 Arc.

## Development

The original disassembly and ammunition features were reported working in-game. **The new automatic junk-selling feature has not yet been verified in Cyberpunk 2077 2.50.** Back up the save before testing:

1. Launch a save with old low-tier weapons and verify they remain untouched.
2. Enable auto-disassembly and one tier, loot a non-iconic weapon of that tier; verify only that weapon is disassembled and components are awarded.
3. Loot an iconic, quest, equipped, or crafted weapon and verify it is kept.
4. Fire a supported weapon to reach the ammo threshold; verify one vanilla batch is crafted, components decrease, and ammo limits are honored.
5. Enable auto-sell junk with several ordinary junk and jewelry items, check the exact eddies received against inventory sell prices. Confirm both existing and newly picked-up junk are handled.\n6. Confirm quest/protected items, ammunition, components and weapons are not sold. Disable auto-sell and verify remaining junk is untouched.\n7. Reload, change saves, toggle settings, open shops/crafting menus, and switch weapons while testing for no unexpected processing.
