#!/usr/bin/env python3
"""Static contracts for Arc Converter; these are not in-game tests."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
moddir = root / "red4ext/plugins/m1arc/mods/m1nsook.arc-converter"
source = (root / "r6/scripts/ArcConverter/ArcConverter.reds").read_text(encoding="utf-8")
junk_source = (root / "r6/scripts/ArcConverter/ArcConverterJunk.reds").read_text(encoding="utf-8")
manifest = json.loads((moddir / "mod.json").read_text(encoding="utf-8"))
settings = json.loads((moddir / "settings.json").read_text(encoding="utf-8"))
assert manifest["format"] == 1
assert manifest["id"] == moddir.name
assert manifest["name"] == "Arc Converter - Automatic Item Processing"
assert manifest["requires"]["m1arc"] == "1.0.0"
assert manifest["version"] == "0.3.0"
assert manifest["scope"] == "Crafting"

ids = [s["id"] for s in settings]
assert len(ids) == len(set(ids))
expected_ids = {
    "enabled", "disassemble", "weapon_action", "tier1", "tier2", "tier3",
    "tier4", "tier5", "clothes_enabled", "clothes_action",
    *(f"clothes_tier{i}" for i in range(1, 6)),
    "ammo", "threshold", "sell_junk",
}
assert set(ids) == expected_ids
by_id = {s["id"]: s for s in settings}
assert by_id["enabled"]["default"] is True
# Keep the original saved weapon setting IDs and their defaults.
assert by_id["disassemble"]["default"] is False
assert all(by_id[f"tier{i}"]["default"] is False for i in range(1, 6))
assert by_id["clothes_enabled"]["default"] is False
assert all(by_id[f"clothes_tier{i}"]["default"] is False for i in range(1, 6))
for key in ("weapon_action", "clothes_action"):
    assert by_id[key]["kind"] == "choice"
    assert by_id[key]["default"] == "Disassemble"
    assert by_id[key]["choices"] == ["Disassemble", "Sell"]
assert by_id["disassemble"]["group"] == "Weapons"
assert by_id["clothes_enabled"]["group"] == "Clothes"
assert all(by_id[f"tier{i}"]["group"] == "Weapons" for i in range(1, 6))
assert all(by_id[f"clothes_tier{i}"]["group"] == "Clothes" for i in range(1, 6))
assert by_id["ammo"]["default"] is True
assert by_id["sell_junk"]["default"] is False
assert by_id["threshold"]["default"] == 25
assert 1 <= by_id["threshold"]["min"] < by_id["threshold"]["max"] <= 100

for setting in settings:
    assert setting["kind"] in {"boolean", "integer", "choice"}
    assert setting["label"] and setting["group"] and setting["description"]
    assert setting["id"] in source + junk_source

for locale in ("en_US", "pl_PL", "ko_KR"):
    translation = json.loads((moddir / "language" / f"{locale}.json").read_text(encoding="utf-8"))
    assert set(translation["settings"]) == expected_ids
    assert translation["name"]
    for sid in ids:
        assert translation["settings"][sid]["label"]
        assert translation["settings"][sid]["group"]
        assert translation["settings"][sid]["description"]
    for key in ("weapon_action", "clothes_action"):
        translated = translation["settings"][key]["choices"]
        assert set(translated) == {"Sell", "Disassemble"}
        assert all(translated.values())

original_ammo_checks = [
    'new CraftItemRequest()',
    'crafting.CanItemBeCrafted(record)',
    'CraftingSystem.GetAmmoBulletAmount(ammoID)',
    'current + batch > capacity',
    'this.CraftEquippedAmmo();',
    'M1Arc_GetBool("m1nsook.arc-converter", "ammo")',
]
for check in original_ammo_checks:
    assert check in source, check

processing_checks = [
    'extends InventoryScriptCallback',
    'OnItemAdded(item: ItemID',
    'this.m_warmupTicks > 0',
    'this.IsMenuOpen()',
    'RPGManager.IsItemWeapon(item)',
    'RPGManager.IsItemClothing(item)',
    'UIInventoryItemsManager.IsItemTypeMeleeWeapon(itemType)',
    'UIInventoryItemsManager.IsItemTypeRangedWeapon(itemType)',
    'UIInventoryItemsManager.IsItemTypeCloting(itemType)',
    'this.IsWeapon(item)',
    'this.IsClothes(item)',
    'data.HasTag(n"base_fists")',
    'M1Arc_GetBool("m1nsook.arc-converter", "disassemble")',
    'M1Arc_GetBool("m1nsook.arc-converter", "clothes_enabled")',
    'M1Arc_GetString("m1nsook.arc-converter", "weapon_action")',
    'M1Arc_GetString("m1nsook.arc-converter", "clothes_action")',
    'this.SelectedTier(item, RPGManager.GetItemQuality(data))',
    'RPGManager.IsItemIconic(data)',
    'RPGManager.IsItemCrafted(data)',
    'RPGManager.IsItemEquipped(this.m_player, item)',
    'crafting.CanItemBeDisassembled(this.m_player, item)',
    'new DisassembleItemRequest()',
    'this.IsSafeEquipment(item, data)',
    'RPGManager.CalculateSellPrice(',
    'ts.RemoveItem(this.m_player, item, 1)',
    'ts.GiveItemByTDBID(this.m_player, t"Items.money", price)',
    'this.m_processingSale = true',
]
for check in processing_checks:
    assert check in source, check
assert 'ts.GetItemList(this.m_player,' not in source, "Equipment must never scan old inventory"
assert 'this.ProcessEquipment();' in source
assert 'continue;' not in source and 'continue;' not in junk_source
assert 'while i < ArraySize(items)' in source

junk_checks = [
    'M1Arc_GetBool("m1nsook.arc-converter", "sell_junk")',
    'ts.GetItemList(this.m_player, items)',
    'gamedataItemType.Gen_Junk',
    'gamedataItemType.Gen_Jewellery',
    'data.HasTag(n"Quest")',
    'data.HasTag(n"UnequipBlocked")',
    'RPGManager.IsItemIconic(data)',
    'RPGManager.CalculateSellPrice(',
    'price <= 0',
    'ts.RemoveItem(this.m_player, item, quantity)',
    'ts.GiveItemByTDBID(this.m_player, t"Items.money", payment)',
]
for check in junk_checks:
    assert check in junk_source, check
assert 'weapon_action' not in junk_source
assert 'clothes_action' not in junk_source
print("PASS: M1 Arc choice settings, safety checks, translations and isolated junk logic")
