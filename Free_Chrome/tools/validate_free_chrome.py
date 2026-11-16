#!/usr/bin/env python3
"""Static contract checks only. This does not compile REDscript or run the game."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
mod = root / "Free_Chrome"
cet = mod / "bin/x64/plugins/cyber_engine_tweaks/mods/FreeChrome"
patch = (cet / "patch.lua").read_text(encoding="utf-8")
main = (cet / "init.lua").read_text(encoding="utf-8")
reds = (mod / "r6/scripts/FreeChrome/FreeChromeSettings.reds").read_text(encoding="utf-8")
workflow = (root / ".github/workflows/release.yml").read_text(encoding="utf-8")
metadata = json.loads((root / ".github/mod-descriptions.json").read_text(encoding="utf-8"))

def require(condition, message):
    if not condition:
        raise AssertionError(message)

for snippet in (
    'ZERO_VARIANT = "Variants.Humanity0Cost"',
    '"gamedataItem_Record"',
    'record:EquipArea()',
    'record:ItemType():Type()',
    '"Cyberware"',
    'record:GetID()',
    'record:GetRecordID()',
    'itemType == gamedataItemType.Cyberware',
    'idName(value)',
    'TDBID.ToStringDEBUG(id)',
    'name .. "." .. property',
    'TweakDB:GetFlat(path)',
    'TweakDB:SetFlat(path, variants)',
    'zeroCostVariants(current)',
    'name:match("^Variants%.Humanity%d+Cost")',
    'matching Humanity variants=',
    'sameArray(safeGetFlat(id, "variants"), variants)',
    'if setVerified(id, zero) then',
    'if setVerified(original.id, original.variants) then',
    'self.originals[key] = { id = id, variants = current }',
    'self.logger("WARNING: no readable Humanity cost variants found; scanner did not modify costs")',
):
    require(snippet in patch, f"patcher missing {snippet}")

for snippet in (
    'registerForEvent("onInit"',
    'registerForEvent("onUpdate"',
    'registerHotkey(MOD .. "_Toggle"',
    'GetMod, "nativeSettings"',
    'api.addSwitch(',
    'if scanClock >= 30 then',
    'if syncClock >= 0.5 then',
    'qs:SetFactStr(FACT, value)',
    'qs:GetFactStr(FACT)',
    'patch:setEnabled(enabled)',
    'patch:scan()',
    'state.settings.enabled = data.settings.enabled',
):
    require(snippet in main, f"settings bridge missing {snippet}")

require('"miso_free_chrome_disabled"' in reds and '"miso_free_chrome_disabled"' in main,
        "REDscript and CET must share the same quest fact")
require('ModSettings.RegisterListenerToClass(s)' in reds
        and 'ModSettings.UnregisterListenerToClass(s)' in reds,
        "Mod Settings should be optional and unregister correctly")
require('ModuleExists("ModSettingsModule")' in reds, "Mod Settings must be optional")

require('Free_Chrome' in metadata and metadata['Free_Chrome']['name'].startswith("Free Chrome"),
        "Missing Free Chrome release metadata")
require('          - Free_Chrome\n' in workflow, "Missing manual release selection")
require('workflow_dispatch:' in workflow and 'push:' not in workflow and 'pull_request:' not in workflow,
        "New mod must not add automatic GitHub Actions triggers")

# No config/saves are committed into install path or included in future ZIPs.
require(not (cet / "db.json").exists(), "Do not commit player settings")
for name in ("FreeChrome/settings.json", "FreeChrome/userdata.json"):
    require(not (cet / name).exists(), "Do not ship user state")

# No stealthy changes to cyberware pricing, stats, unlocks or capacity maximum.
for forbidden in (
    '".buyPrice"', '".sellPrice"', '".statModifiers"', '".CyberwareCapacity"',
    'BaseStats.Humanity', 'Game.GetStatsSystem():AddModifier',
):
    require(forbidden not in patch, f"Unrelated game stat mutation: {forbidden}")
print("PASS: Free Chrome zero-cost patching, reversible original values, settings, and manual-only release")
