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
assert "wrappedMethod(e)" in addon and "GameInstance.GetTeleportationFacility" in addon
assert not any(s in addon for s in ("RegisterMappin(", "UnregisterMappin(", "SetFactStr("))
assert 'MOD .. "_Status"' not in cet and 'MOD .. "_Preview"' not in cet
assert "miso_rmp_diag_seen" not in core
print("PASS: optional teleporter owner check, hold input and core cleanup")
