#!/usr/bin/env python3
"""Check the optional native UndiscoverMe filter extension against source files.

No game, REDscript compiler, third-party Python modules, or saves required.
This is a static safety test, NOT proof the map UI compiles/works in-game.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ui = (root / "r6/scripts/RevealMapPins/UndiscoverMeMapFilters.reds").read_text(encoding="utf-8")
tweak = (root / "r6/tweaks/UndiscoverMe/filters.yaml").read_text(encoding="utf-8")
core = (root / "r6/scripts/RevealMapPins/RevealMapPins.reds").read_text(encoding="utf-8")
cet = (root / "bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins/init.lua").read_text(encoding="utf-8")

groups = ["Menu", "All", "NCPD", "Psycho", "Tarot", "Travel"]
assert len(groups) == 6
for group in groups:
    record = "WorldMap.UndiscoverMe" + group
    assert record + ":" in tweak, record
    assert "!prepend " + record in tweak, record
    assert 't"' + record + '"' in ui, record

assert tweak.count("mappins: []") == len(groups), "filter records must never claim native variants"
assert tweak.count("filterType: WorldMap.CustomFilter") == len(groups)
assert "shouldCollectMappins: true" not in tweak.lower()
assert "WorldMap.WorldMapCustomFiltersList.list:" in tweak

# Our click handler must identify owned record IDs and use the original
# game's handler for any other mod/vanilla filter.
assert "row.UDMFilterSlot()" in ui
assert "m_udmExpanded" in ui and "UDMRefreshFilterRows()" in ui
assert "row.GetRootWidget().SetVisible(!this.m_udmExpanded)" in ui
assert "if slot > 0" in ui
assert "wrappedMethod(evt);" in ui
assert "evt.Consume();" in ui
assert "this.UpdateCustomFilter(typ, true);" in ui
for prohibited in ("UnregisterMappin", "RegisterMappin", "ClearCustomFilter",
                   "ChangeEntryState", "FastTravel("):
    assert prohibited not in ui, prohibited

# UI facts are the original working CET/REDscript bridge and only affect
# UndiscoverMe switches. No player save or tracking facts may change.
facts = [
    "miso_rmp_all_disabled", "miso_rmp_catalog_enabled",
    "miso_rmp_ncpd_disabled", "miso_rmp_psycho_disabled",
    "miso_rmp_tarot_disabled", "miso_rmp_travel_disabled",
]
for fact in facts:
    assert fact in ui and fact in cet or fact == "miso_rmp_catalog_enabled" and fact in ui and fact in cet
assert 'local MOD = "RevealMapPins"' in cet, "CET IDs must remain backward compatible"
assert 'local TITLE = "UndiscoverMe"' in cet
assert "ModSettings.mod\", \"UndiscoverMe" in core
assert "MisoRevealMapPins" in core and "MisoRevealMapPins" in ui
print("PASS: five native UndiscoverMe shortcuts, scoped handlers, compatible settings")
