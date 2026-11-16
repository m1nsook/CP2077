#!/usr/bin/env python3
"""Source checks, not a game compile."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
core=(root/"UndiscoverMe/r6/scripts/UndiscoverMe/UndiscoverMe.reds").read_text()
runtime=(root/"UndiscoverMe/bin/x64/plugins/cyber_engine_tweaks/mods/UndiscoverMe/catalog_runtime.lua").read_text()
cet=(root/"UndiscoverMe/bin/x64/plugins/cyber_engine_tweaks/mods/UndiscoverMe/init.lua").read_text()
addon=(root/"UndiscoverMe_Teleporter/r6/scripts/UndiscoverMeTeleporter/UndiscoverMeTeleporter.reds").read_text()
assert "UDMOwnsPin(pin.GetNewMappinID())" in addon
assert all(v in core for v in ("UDMRegisterCatalogPin", "UDMUnregisterCatalogPin", "UDMOwnsPin"))
assert "onOwnedChanged(id, false)" in runtime
assert "onOwnedChanged(self.owned[entry.hash], true)" in runtime
assert "world_map_menu_fast_travel" in addon and "e.GetHoldProgress() >= 1.0" in addon
assert "UDMCanTeleportPin(controller.GetMappin())" in addon
assert "m_udmHeldPinID" not in addon, "Extra press gate blocks native hold callbacks"
assert "this.m_startedFastTraveling = true" not in addon, "Do not alter vanilla hold flags"
assert "protected cb func OnPressInput(e: ref<inkPointerEvent>)" in addon
assert "protected cb func OnHoldInput(e: ref<inkPointerEvent>)" in addon
assert "protected cb func OnReleaseInput(e: ref<inkPointerEvent>)" in addon
assert "if e.IsAction(n\"world_map_menu_fast_travel\") && this.UDMCanTeleportSelected()" in addon
assert "tooltipData.fastTravelEnabled = true" in addon
assert "this.m_tooltipController.SetData(tooltipType, tooltipData, this)" in addon
assert "menu.UDMCanTeleportPin(Deref(data).mappin)" in addon
assert "inkWidgetRef.SetVisible(this.m_inputInteractContainer" in addon
assert "UDMCanTeleportPin(pin.GetNewMappinID())" not in addon
assert "this.UDMCanTeleportPin(this.selectedMappin.GetMappin())" in addon
assert "wrappedMethod(e)" in addon and "GameInstance.GetTeleportationFacility" in addon
assert not any(s in addon for s in ("RegisterMappin(", "UnregisterMappin(", "SetFactStr("))
assert 'MOD .. "_Status"' not in cet and 'MOD .. "_Preview"' not in cet
assert "miso_rmp_diag_seen" not in core
print("PASS: owned-pin native tooltip, restored working hold input and core cleanup")
