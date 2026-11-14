#!/usr/bin/env python3
"""Static safety gate for UndiscoverMe's separate top-level map quick filter.

These tests cannot prove REDscript compiles or that the game menu renders.
"""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ui = (root / "r6/scripts/UndiscoverMe/UndiscoverMeMapFilters.reds").read_text(encoding="utf-8")
tweak = (root / "r6/tweaks/UndiscoverMe/filters.yaml").read_text(encoding="utf-8")
core = (root / "r6/scripts/UndiscoverMe/UndiscoverMe.reds").read_text(encoding="utf-8")
cet = (root / "bin/x64/plugins/cyber_engine_tweaks/mods/UndiscoverMe/init.lua").read_text(encoding="utf-8")
runtime = (root / "bin/x64/plugins/cyber_engine_tweaks/mods/UndiscoverMe/catalog_runtime.lua").read_text(encoding="utf-8")

# Only one new quick-filter group, after the four original groups.
assert "WorldMap.WorldMapQuickFiltersList.list:" in tweak
assert "!append-once WorldMap.UndiscoverMeQuickFilter" in tweak
assert "WorldMap.UndiscoverMeQuickFilter:" in tweak
assert "filterType: WorldMap.CustomFilter" in tweak
assert "mappins: []" in tweak
assert "WorldMap.WorldMapCustomFiltersList.list:" not in tweak
assert "!prepend" not in tweak

# Five real INK indicators: the vanilla fixed array remains size four, but
# a safe native sibling is created for the extra quick filter. No out-of-bounds
# reference is ever passed to the engine.
assert "SetQuickFilterIndicator(index: Int32)" in ui
assert "if index >= count {" in ui
assert "wrappedMethod(count - 1);" in ui
assert "if ArraySize(this.m_quickFilterIndicators) != 4 { return; }" in ui
assert "UDMEnsureFifthPill()" in ui
assert 'new inkRectangle()' in ui
assert 'source.GetParentWidget() as inkCompoundWidget' in ui
assert 'pill.Reparent(parent)' in ui
assert 'this.m_udmFifthPill.SetState(n"Selected")' in ui
assert 'fourth.SetState(n"Default")' in ui
assert "ToggleQuickFilterIndicatorsVsibility(toggle: Bool)" in ui
assert "this.m_udmFifthPill.SetVisible(toggle)" in ui
assert "this.m_udmFifthPill = null" in ui

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
assert "miso_rmp_catalog_enabled" in ui
assert "miso_rmp_ncpd_disabled" in ui
assert "miso_rmp_psycho_disabled" in ui
assert "miso_rmp_tarot_disabled" in ui
assert "miso_rmp_travel_disabled" in ui

# Keep existing renderer, 212-row catalog, and original fact/binding IDs.
assert 'local MOD = "RevealMapPins"' in cet
assert 'local TITLE = "UndiscoverMe"' in cet
assert "ModSettings.mod\", \"UndiscoverMe" in core
assert "JOURNAL_QUEST_IDS = 175" in runtime
assert "if #rows ~= 212" in runtime
print("PASS: UndiscoverMe has a native fifth pill, separate quick tab, Custom unchanged, safe restore")
