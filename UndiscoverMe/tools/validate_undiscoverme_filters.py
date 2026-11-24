#!/usr/bin/env python3
"""Static safety gate for UndiscoverMe's separate top-level map quick filter.

These tests cannot prove REDscript compiles or that the game menu renders.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ui = (root / "r6/scripts/UndiscoverMe/UndiscoverMeMapFilters.reds").read_text(encoding="utf-8")
tweak = (root / "r6/tweaks/UndiscoverMe/filters.yaml").read_text(encoding="utf-8")
core = (root / "r6/scripts/UndiscoverMe/UndiscoverMe.reds").read_text(encoding="utf-8")

catalog = (root / "r6/scripts/UndiscoverMe/UndiscoverMeCatalog.reds").read_text(encoding="utf-8")

# Only one new quick-filter group, after the four original groups.
assert "WorldMap.WorldMapQuickFiltersList.list:" in tweak
assert "!append-once WorldMap.UndiscoverMeQuickFilter" in tweak
assert "WorldMap.UndiscoverMeQuickFilter:" in tweak
assert "filterType: WorldMap.CustomFilter" in tweak
assert "mappins: []" in tweak
assert "WorldMap.WorldMapCustomFiltersList.list:" not in tweak
assert "!prepend" not in tweak

# The new fifth tab must never index the game's four hard-coded dots.
assert "SetQuickFilterIndicator(index: Int32)" in ui
assert "if index >= count { wrappedMethod(count - 1); return; }" in ui

# Record-identity-based quick tab, not the shared native CustomFilter bit.
assert 'Equals(rec.GetID(), t"WorldMap.UndiscoverMeQuickFilter")' in ui
assert "UDMReconcileQuickTab()" in ui
assert "CycleWorldMapFilter(cycleNext: Bool)" in ui

# UndiscoverMe only lists these supported original native variants.
for filter_type in ("Story", "PsychoFilter", "TarotFilter", "FastTravel"):
    assert "gamedataWorldMapFilter." + filter_type in ui
assert "UDMSlot(row.GetFilterType())" in ui
assert "row.GetRootWidget().SetVisible(slot > 0)" in ui
assert "NavigateCustomFilters(option:" in ui, "controller navigation must skip hidden rows"

# Vanilla Custom filter choices restored even if map closed on new fifth tab.
assert "m_udmSavedCustom" in ui
assert "ArrayPush(this.m_udmSavedCustom" in ui
assert "this.UDMLeave();" in ui
assert "OnUninitialize()" in ui
assert "wrappedMethod();" in ui
assert "if !this.m_udmQuickActive" in ui

# No new game marker, fact progression, or native marker deletion.
for prohibited in ("RegisterMappin", "UnregisterMappin", "ChangeEntryState",
                   "CompleteQuest", "Mappins.CustomMapMarkersVariant"):
    assert prohibited not in ui, prohibited
for key in ("enabled", "catalog", "ncpd", "psycho", "tarot", "travel"):
    assert f'RMPState.Get("{key}")' in ui
assert 'M1Arc_SetBool("m1nsook.undiscoverme"' in ui
assert 'M1Arc_GetBool("m1nsook.undiscoverme"' in core
assert catalog.count("ArrayPush(rows, UDMCatalogRow.Make(") == 212
assert not list(root.glob("bin/**/*.lua"))
assert "journal.GetQuests(context, finished)" in core
assert "GetPointOfInterestMappinSavedState" in core
assert "UDMOwnsPin(pin.GetNewMappinID())" in core
assert "miso_rmp_all_disabled" in core
print("PASS: fifth tab, native catalog and M1 Arc settings")
