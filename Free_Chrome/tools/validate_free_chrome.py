#!/usr/bin/env python3
"""Static Free Chrome source checks. Game/runtime verification is separate."""
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
    "MAX_HUMANITY_COST = 200",
    '"Variants.Humanity" .. cost .. "Cost"',
    '"TinkererVariant"',
    '"_inline" .. index .. ".value"',
    'TweakDB:GetFlat(path)',
    'TweakDB:SetFlat(path, value)',
    'getNumber(path) == value',
    'self.originals[path] = value',
    'if current == 0 then',
    'setNumber(path, oldValue)',
    'self.additional[path] = true',
    'function self:addCostFlat(path)',
    'Humanity modifier fields found=',
    'write failures=',
):
    require(snippet in patch, "shared Humanity modifier logic missing: " + snippet)

require('gamedataItem_Record' not in patch, "Do not use broken item-record scan")
require('TweakDBID.new' not in patch, "Do not replace item variant array")
require('".variants"' not in patch, "Do not change implant variants")
require('MAX_HUMANITY_COST = 200' in patch, "Unexpected Humanity scan bound")

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
    require(snippet in main, "settings bridge missing: " + snippet)

require('"miso_free_chrome_disabled"' in reds
        and '"miso_free_chrome_disabled"' in main, "Settings fact mismatch")
require('ModSettings.RegisterListenerToClass(s)' in reds and
        'ModSettings.UnregisterListenerToClass(s)' in reds,
        "Mod Settings registration missing")
require('ModuleExists("ModSettingsModule")' in reds, "Mod Settings must be optional")

require('Free_Chrome' in metadata, "Release metadata missing")
require('          - Free_Chrome\n' in workflow, "Manual release dropdown missing")
require('workflow_dispatch:' in workflow and 'push:' not in workflow and
        'pull_request:' not in workflow, "Do not add automatic Actions triggers")
require(not (cet / "db.json").exists(), "Do not ship player settings")
print("PASS: shared Humanity modifier zeroing, read-back, reversible settings, and manual packaging")
