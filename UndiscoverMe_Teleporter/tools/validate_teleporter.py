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
assert "world_map_menu_fast_travel" in addon
assert "private final func HandlePressInput(e: ref<inkPointerEvent>) -> Void" in addon
assert "private final func TryFastTravel() -> Void" in addon
assert "this.m_startedFastTraveling = true" in addon
assert "this.m_isPanning = true" in addon
assert "this.m_audioSystem.Play(n\"ui_menu_item_crafting_done\")" in addon
# A direct world-position teleport is not enough: the fast-travel transition
# must explicitly close the hub menu and restore first-person camera state.
assert "nextLoadingTypeEvt.SetNextLoadingScreenType(inkLoadingScreenType.FastTravel)" in addon
assert "this.QueueBroadcastEvent(nextLoadingTypeEvt)" in addon
assert "this.QueueBroadcastEvent(new ForceCloseHubMenuEvent())" in addon
assert 'this.m_menuEventDispatcher.SpawnEvent(n"OnBack")' in addon
assert "playerPuppet.GetFPPCameraComponent().ResetPitch()" in addon
assert 'this.m_menuEventDispatcher.SpawnEvent(n"OnCloseHubMenu")' not in addon
assert "wrappedMethod();" in addon, "Non-UndiscoverMe native travel must pass through"
assert "UDMCanTeleportPin(controller.GetMappin())" in addon
assert "m_udmHeldPinID" not in addon, "Extra user-defined hold gate is unnecessary"
assert "func OnPressInput(" not in addon and "func OnHoldInput(" not in addon
assert "func OnReleaseInput(" not in addon
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
print("PASS: owned-pin tooltip, native hold, full travel menu/camera cleanup and core guards")
