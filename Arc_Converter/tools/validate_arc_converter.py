#!/usr/bin/env python3
"""Static contracts for Arc Converter (not a substitute for in-game testing)."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
moddir = root / "red4ext/plugins/m1arc/mods/m1nsook.arc-converter"
weapon_source = (root / "r6/scripts/ArcConverter/ArcConverter.reds").read_text(encoding="utf-8")
junk_source = (root / "r6/scripts/ArcConverter/ArcConverterJunk.reds").read_text(encoding="utf-8")
source = weapon_source + "\n" + junk_source
manifest = json.loads((moddir / "mod.json").read_text(encoding="utf-8"))
settings = json.loads((moddir / "settings.json").read_text(encoding="utf-8"))
assert manifest["format"] == 1
assert manifest["id"] == moddir.name
assert manifest["name"] == "Arc Converter - Automatic Item Processing"
assert manifest["requires"]["m1arc"] == "1.0.0"
assert manifest["scope"] == "Weapons"
ids = [s["id"] for s in settings]
assert len(ids) == len(set(ids))
assert set(ids) == {
    "enabled", "disassemble", "tier1", "tier2", "tier3", "tier4", "tier5",
    "ammo", "threshold", "sell_junk",
}
by_id = {s["id"]: s for s in settings}
assert by_id["enabled"]["default"] is True
assert by_id["disassemble"]["default"] is False
assert all(by_id[f"tier{i}"]["default"] is False for i in range(1, 6))
assert by_id["ammo"]["default"] is True
assert by_id["sell_junk"]["default"] is False
assert by_id["sell_junk"]["kind"] == "boolean"
assert by_id["threshold"]["default"] == 25
assert 1 <= by_id["threshold"]["min"] < by_id["threshold"]["max"] <= 100
for setting in settings:
    assert setting["kind"] in {"boolean", "integer"}
    assert setting["id"] in source

for loc in ("en_US", "pl_PL", "ko_KR"):
    translation = json.loads((moddir / "language" / f"{loc}.json").read_text(encoding="utf-8"))
    assert set(translation["settings"]) == set(ids)
    assert translation["name"]
    for sid in ids:
        assert translation["settings"][sid]["label"]
        assert translation["settings"][sid]["group"]
        assert translation["settings"][sid]["description"]

checks = [
    'extends InventoryScriptCallback',
    'OnItemAdded(item: ItemID',
    'this.m_warmupTicks > 0',
    'this.IsMenuOpen()',
    'RPGManager.IsItemIconic(data)',
    'RPGManager.IsItemCrafted(data)',
    'RPGManager.IsItemEquipped(this.m_player, item)',
    'crafting.CanItemBeDisassembled(this.m_player, item)',
    'new DisassembleItemRequest()',
    'new CraftItemRequest()',
    'crafting.QueueRequest(request)',
    'crafting.CanItemBeCrafted(record)',
    'CraftingSystem.GetAmmoBulletAmount(ammoID)',
    'current + batch > capacity',
]
for needle in checks:
    assert needle in source, needle
for ammo in ("HandgunAmmo", "RifleAmmo", "ShotgunAmmo", "SniperRifleAmmo"):
    assert f't"Ammo.{ammo}"' in source
for forbidden in ("ts.RemoveItem(", "ts.GiveItem(", "transactionSystem.GiveItem(", "transactionSystem.RemoveItem(", "RemoveAllItems(", "RemoveAllModifiers("):
    assert forbidden not in weapon_source, forbidden
junk_checks = [
    'extends ScriptableSystem',
    'm_warmupTicks = 3',
    'this.IsMenuOpen()',
    'M1Arc_GetBool("m1nsook.arc-converter", "sell_junk")',
    'ts.GetItemList(this.m_player, items)',
    'gamedataItemType.Gen_Junk',
    'gamedataItemType.Gen_Jewellery',
    'data.HasTag(n"Quest")',
    'data.HasTag(n"UnequipBlocked")',
    'RPGManager.IsItemIconic(data)',
    'RPGManager.CalculateSellPrice(',
    'price <= 0',
    'ts.GetItemQuantity(this.m_player, item)',
    'ts.RemoveItem(this.m_player, item, quantity)',
    'ts.GiveItemByTDBID(this.m_player, t"Items.money", payment)',
    'ts.GiveItem(this.m_player, item, quantity)',
]
for needle in junk_checks:
    assert needle in junk_source, needle
# Keep junk conversion isolated: the weapon-only source must not contain sales.
assert "continue;" not in junk_source, "REDscript has no continue statement"
assert 'sell_junk' not in weapon_source
assert 'ts.RemoveItem(' not in weapon_source
assert not (root / "bin").exists()
print("PASS: manifest, settings, translations and static safety contracts")
