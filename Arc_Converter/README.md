# Arc Converter - Automatic Item Processing

Cyberpunk 2077 2.50 mod using RED4ext, redscript and M1 Arc 1.0.0. No CET dependency.

## Features

**Weapons (independent on/off)**

- Decide how to process newly acquired weapons: **Sell** (for eddies) or **Disassemble** (for components).
- Select exactly which tiers to process, Tier 1–5 including their + variants. Includes ranged weapons and melee weapons (knives, katanas, swords, axes and blunt weapons).
- Previously acquired weapons are never scanned or automatically processed.
- Equipped, iconic, quest, crafted, locked and otherwise protected weapons are kept.

**Clothes (independent on/off)**

- Select **Sell** or **Disassemble** and individual Tier 1–5 checkboxes.
- Only newly acquired clothing is eligible; the existing wardrobe/inventory is not scanned.
- Equipped, iconic, quest, crafted, locked and otherwise protected items are kept.
- Disassembly uses the game's original crafting/disassembly request.

**Ammunition**

- Auto-crafts vanilla handgun, rifle, shotgun and sniper ammunition for the equipped weapon when ammunition falls below a configurable percentage.
- Uses real crafting components, the game recipes and vanilla capacity limits.

**Junk**

- Optionally exchanges regular junk and jewelry for eddies at the game's selling prices without a vendor.
- Unlike the Weapons and Clothes processors, it **does** handle junk already in V's inventory.

## M1 Arc settings and upgrades

| Section | Settings | Default |
| --- | --- | --- |
| General | Enable Arc Converter | On |
| Weapons | Weapons on/off | Off (saved setting ID `disassemble`) |
| Weapons | Weapon action: Disassemble / Sell | Disassemble |
| Weapons | Tier 1, 2, 3, 4, 5 | Off each |
| Clothes | Clothes on/off | Off |
| Clothes | Clothes action: Disassemble / Sell | Disassemble |
| Clothes | Tier 1, 2, 3, 4, 5 | Off each |
| Ammunition | Auto craft; threshold | On; 25% |
| Junk | Automatically sell junk | Off |

The original Weapons toggle (stored as `disassemble`) and existing `tier1`–`tier5` IDs are **preserved** so upgrading does not reset your selections. New modes and clothing controls start at safe defaults. English, Polish and Korean UI strings are provided.

## How processing works

- A weapon or clothing item is registered when the inventory callback reports a **new addition** outside the game menus. No old-item or backpack sweep takes place.
- Melee weapons are recognized by their actual game item type as well as the Weapon category, so knives and swords are not skipped by generic category classification.\n- The mod checks its tier and protection status again before doing anything.
- **Disassemble** uses `CraftingSystem.CanItemBeDisassembled` and the vanilla `DisassembleItemRequest` so materials come from the normal game recipe.
- **Sell** uses `RPGManager.CalculateSellPrice` and exchanges the item for `Items.money` only if removal succeeds. It does not visit a vendor, consume vendor funds, or add items to a vendor's buyback stock.
- Silent internal item grants and acquisitions while game menus are open are skipped. The inventory callback does not identify the precise source (e.g. enemy loot versus other inventory grants).
- Changing a switch or selected tiers takes effect on the next eligible processing check.

The independent junk processor scans junk and jewelry in V's backpack (including old stacks), up to a bounded number per tick. It skips items with no positive selling price, avoids protected items, and attempts to restore an item if crediting currency fails.

## Installation

Install into the Cyberpunk 2077 game root:

- `r6/scripts/ArcConverter/ArcConverter.reds`
- `r6/scripts/ArcConverter/ArcConverterJunk.reds`
- `red4ext/plugins/m1arc/mods/m1nsook.arc-converter/`

Open **Arc Converter - Automatic Item Processing** in M1 Arc to configure your settings.

## Release 1.0.0 — tested in game

Arc Converter's weapon and clothes Sell / Disassemble modes, melee weapon support, ammunition crafting and junk conversion have been tested in Cyberpunk 2077 2.50.

Nexus Mods: https://www.nexusmods.com/games/cyberpunk2077/mods/39454

## 1.0.1 development update — runtime testing pending

- Junk: Sell (existing default), Disassemble, or By value. The threshold is per item: at or below it disassemble, above it sell. Unsupported disassembly is skipped, not sold automatically.
- Food/drinks and boosters: independent switches and per-tier Keep/Sell/Disassemble choices. Includes existing stacks; each stack retains a configurable reserve (default 1). Healing inhalers/injectors and skill shards are excluded. Equipped, quest and protected items are skipped. Disassembly requires vanilla eligibility; ordinary food/drinks may not support it without a separate mod restoring that action. No artificial component rewards.
- Optional Much Better Ammo 1.8 support: uses its router for capacities (including empty stacks), four custom ammo records, and game crafting recipes/yields. No required dependency or redistributed files. When present its capacity settings are honored even for the four standard ammo types.
- Optional Expanded Dual Wielding 3.8 support: native left/right active weapon APIs, distinct ammo pools, alternating priority, one recipe per cycle. Works with or without Much Better Ammo. Both optional switches default off.
- Existing settings IDs and the existing junk Sell default are preserved. No changes to release workflows.
