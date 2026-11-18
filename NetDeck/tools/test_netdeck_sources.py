#!/usr/bin/env python3
"""Static source invariants; intentionally NOT a redscript compilation test."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
R = (ROOT / "r6/scripts/NetDeck/NetDeck.reds").read_text("utf-8")
Y = (ROOT / "r6/tweaks/NetDeck/NetDeck.yaml").read_text("utf-8")
L = (ROOT / "r6/scripts/NetDeck/Localization.reds").read_text("utf-8")
D = (ROOT / "bin/x64/plugins/cyber_engine_tweaks/mods/NetDeckDiagnostics/init.lua").read_text("utf-8")
P = (ROOT / "tools/pack_test.py").read_text("utf-8")
VENDORS = 26

class InfiltrationSourceTests(unittest.TestCase):
    def test_crash_diagnostic_is_observer_only(self):
        self.assertIn('PREFIX = "[NetDeck/DIAG v0.6.6]"', D)
        self.assertIn('registerForEvent("onInit"', D)
        self.assertIn('registerForEvent("onUpdate"', D)
        self.assertIn('registerForEvent("onShutdown"', D)
        self.assertIn('ObserveAfter(className, methodName', D)
        self.assertIn('observe("PlayerPuppet", "NetDeckWaveStart"', D)
        self.assertIn('observe("PlayerPuppet", "NetDeckWaveInfect"', D)
        self.assertIn('observe("PlayerPuppet", "NetDeckWaveFinish"', D)
        self.assertIn('observe("PlayerPuppet", "NetDeckHudStart"', D)
        self.assertIn('observe("PlayerPuppet", "NetDeckHudFinish"', D)
        # Don't observe globally executed game streaming callbacks.
        self.assertNotIn('ObserveAfter("VehicleObject"', D)
        self.assertNotIn('ObserveAfter("NPCPuppet"', D)
        self.assertNotIn('Game.GetTargetingSystem', D)
        self.assertNotIn('GetWorldPosition', D)
        # ACR captures CET scripting.log without a crash-reporter plugin API.
        self.assertIn('bin/x64/plugins/cyber_engine_tweaks/mods/NetDeckDiagnostics/init.lua', P)

    def test_save_compatible_item_and_stats(self):
        for s in ["Items.NESTCyberdeck:", "$base: Items.AdvancedNetwatchNetdriverMKLegendaryPlusPlus",
                  "Items.CyberdeckBlueprint4Slots", "Variants.Humanity5Cost",
                  "statType: BaseStats.Memory", "value: 15",
                  "statType: BaseStats.BufferSize", "value: 4"]:
            self.assertIn(s, Y)

    def test_names_are_display_only_and_save_compatible(self):
        self.assertIn('friendlyName: "NetWatch Σ MK.1"', Y)
        self.assertIn('this.Text("NetDeckDeckName", "NetWatch Σ MK.1")', L)
        self.assertIn('this.Text("NetDeckInfiltrationName", "NETWORM")', L)
        self.assertIn('caption: l"NetDeckInfiltrationName"', Y)
        self.assertIn('Items.NESTCyberdeck:', Y)
        self.assertIn('actionName: NetDeckInfiltration', Y)
        self.assertNotIn('this.Text("NetDeckInfiltrationName", "Infiltration")', L)

    def test_vendor_coverage(self):
        self.assertEqual(len(set(re.findall(r"^Vendors\.(\w+)\.itemStock:", Y, re.M))), VENDORS)
        self.assertIn("Vendors.AdvancedCollectiveRipperDocDogtown.itemStock:", Y)
        self.assertIn("Vendors.Always_Present", Y)

    def test_native_action_gate(self):
        for s in ["module NetDeck", "class NetDeckInfiltration extends ActionBool",
                  "FinalizeGetQuickHackActions(", "GetPlayerQuickHackListWithQuality(",
                  "PlayerQuickhackData(ItemID.None(), record, 5)",
                  "DeviceAction.NetDeckInfiltration",
                  "QuickHack.NetDeckPuppetInfiltration",
                  "GetAllChoices(",
                  "OnNetDeckPuppetInfiltration",
                  "protected cb func OnNetDeckInfiltration"]:
            self.assertIn(s, R)
        self.assertNotIn("public func GetRemoteActions(", R)
        self.assertNotIn("@wrapMethod(AccessPointControllerPS)", R)
        self.assertNotIn("this as AccessPointController;", R)
        self.assertNotIn("@wrapMethod(Device)", R)
        self.assertNotIn("member as PuppetDeviceLinkPS", R)
        self.assertIn("outActions: script_ref<array<ref<DeviceAction>>>", R)
        self.assertIn("const context: script_ref<GetActionsContext>", R)
        self.assertIn("protected final func FinalizeGetQuickHackActions(", R)
        self.assertIn("wrappedMethod(outActions, context);", R)
        self.assertIn("Items.NESTCyberdeck", R)
        self.assertIn("NetDeckInfiltration", R)

    def test_network_safety(self):
        for s in ["this.GetAccessPoints()", "GetAllDescendants(members)",
                  "ArrayContains(seen, member)", "member.IsMarkedAsQuest()",
                  "SurveillanceCameraControllerPS", "SecurityTurretControllerPS",
                  "ExplosiveTriggerDeviceControllerPS", "QuickHackDistraction",
                  "member.GetQuickHackActions(actions, context)"]:
            self.assertIn(s, R)
        self.assertNotIn("SystemCollapse", R)
        self.assertIn("member.NetDeckApplyToMember(member)", R)
        self.assertIn("this.NetDeckApplyToMember(this)", R)
        self.assertIn("this.NetDeckInfiltrateMaster(directMaster, seen)", R)
        self.assertIn("class NetDeckPuppetInfiltration extends PuppetAction", R)
        self.assertIn("public final const func GetAllChoices(", R)
        self.assertIn("PuppetDeviceLinkPS", R)
        self.assertNotIn("member as PuppetDeviceLinkPS", R)
        self.assertIn("QuickHack.NetDeckPuppetInfiltration:", Y)
        self.assertIn("ObjectActionType.PuppetQuickHack", Y)
        self.assertIn("DeviceAction.NetDeckInfiltration:", Y)
        self.assertNotIn("$base: DeviceAction.PingDevice", Y)
        self.assertNotIn("$base: Interactions.PingHack", Y)
        self.assertNotIn('IsDefined(this.NetDeckNetworkMaster()) {', R)

    def test_valid_native_target_mask(self):
        # TSFMV is the Cyberpunk targeting mask; it has Obj_Other, not Obj_Vehicle.
        self.assertNotIn("TSFMV.Obj_Vehicle", R)
        self.assertIn("TSFMV.Obj_Puppet | TSFMV.Obj_Device | TSFMV.Obj_Other", R)

    def test_timed_wave_spreading(self):
        self.assertNotIn("@wrapMethod(AccessPointControllerPS)", R)
        self.assertNotIn("public func GetRemoteActions(", R)
        self.assertIn("!IsDefined(this as AccessPointControllerPS)", R)
        self.assertIn("public class NetDeckWaveTickEvent extends Event", R)
        self.assertIn("@addField(PlayerPuppet)", R)
        self.assertIn("netDeckWaveHosts: array<Vector4>", R)
        self.assertIn("netDeckWaveLinks: array<Int32>", R)
        self.assertIn("netDeckWaveSeen: array<EntityID>", R)
        self.assertIn("netDeckWaveGeneration += 1", R)
        self.assertIn("evt.generation != this.netDeckWaveGeneration", R)
        self.assertIn("NetDeckWaveStart(root: ref<GameObject>, fromDevice: Bool)", R)
        self.assertIn("protected cb func OnNetDeckWaveTickEvent", R)
        self.assertIn("NetDeckWaveInfect(selected, depth)", R)
        self.assertIn("DelayEvent(this, evt, 2.0)", R)
        self.assertIn("GetTargetParts(\n      this, query, parts", R)
        self.assertIn("TargetingSet.Complete", R)
        self.assertIn("Vector4.Distance(this.netDeckWaveHosts[j], candidate.GetWorldPosition())", R)
        self.assertIn("this.netDeckWaveLinks[j] < 4", R)
        self.assertIn("this.netDeckWaveDepths[j] < 4", R)
        self.assertIn("this.netDeckWaveSeen) >= 40", R)
        self.assertIn("this.netDeckWavePendingCursor", R)
        self.assertIn("NetDeckWaveAddMaster", R)
        self.assertNotIn("NetDeckWaveProgress", R)
        self.assertIn("NetDeckTargetsAffected", R)
        self.assertNotIn("public func NetDeckSpread(", R)
        self.assertNotIn("GetTargetParts(obj, query, parts)", R)
        self.assertNotRegex(R, r"\bcontinue\s*;")

    def test_silent_two_second_bounded_wave(self):
        self.assertIn("this.netDeckWaveTicks += 1;", R)
        self.assertIn("this.netDeckWaveTicks > 16", R)
        self.assertNotIn("PingNetworkGrid_Event(", R)
        self.assertNotIn("NetDeckPulseNetwork(ps)", R)
        self.assertNotIn("notification.m_text = loc.GetText(\"NetDeckWaveProgress\")", R)
        self.assertIn("r.robbedTargets, r.npcs + r.vehicles", R)

    def test_experimental_upload_bar_has_quest_ownership_guards(self):
        # The 16:29:37 PL crash predates the 16:30:42 PL HUD commit.
        # This is a SOURCE test only, not a REDscript/gameplay validation.
        self.assertIn("UI_HUDProgressBar", R)
        self.assertIn("private func NetDeckHudStart()", R)
        self.assertIn("private func NetDeckHudUpdate()", R)
        self.assertIn("private func NetDeckHudFinish()", R)
        self.assertIn("if bb.GetBool(d.Active) { return; }", R)
        self.assertIn("!Equals(bb.GetString(d.Header), this.netDeckHudHeader)", R)
        self.assertIn("Equals(bb.GetString(d.Header), this.netDeckHudHeader)", R)
        self.assertIn("this.NetDeckHudFinish();", R)
        self.assertIn("this.NetDeckHudUpdate();", R)
        self.assertIn('loc.GetText("NetDeckUploadProgress")', R)
        # Never restore the repeating network VFX/audio path.
        self.assertNotIn("PingNetworkGrid_Event(", R)

    def test_fast_native_neutral_upload_hud(self):
        # ~3s native HUD upload, not the independent 32s propagation loop.
        # Built-in Neutral type replaces the alert icon/visual state without
        # recoloring the global theme or patching HUDProgressBarController.
        self.assertIn("public class NetDeckHudTickEvent extends Event", R)
        self.assertIn("protected cb func OnNetDeckHudTickEvent(", R)
        self.assertIn("private func NetDeckHudSchedule(step: Int32)", R)
        self.assertIn("DelayEvent(this, evt, 0.15)", R)
        self.assertIn("evt.step >= 20", R)
        self.assertIn("Cast<Float>(evt.step) / 20.0", R)
        self.assertIn("this.NetDeckHudSchedule(1);", R)
        self.assertIn("bb.SetVariant(d.MessageType, SimpleMessageType.Neutral, true);", R)
        self.assertNotIn("this.netDeckHudProgress += 0.0625", R)
        self.assertNotIn("bb.SetString(d.BottomText, loc.GetText(\"NetDeckDevicesAffected\")", R)
        self.assertIn("DelayEvent(this, evt, 2.0)", R)
        self.assertNotIn("@wrapMethod(HUDProgressBarController)", R)
        self.assertNotIn("@wrapMethod(NPCPuppet)", R)
        self.assertNotIn("@wrapMethod(VehicleObject)", R)

    def test_scoped_trace_hook(self):
        self.assertIn("NetDeck.ZeroAwareness:", Y)
        self.assertEqual(Y.count("awarenessCost: NetDeck.ZeroAwareness"), 3)
        self.assertIn("@wrapMethod(QuickHackableHelper)", R)
        self.assertIn("public final static func WillHackRevealPlayer(", R)
        self.assertIn('Equals(record.ActionName(), n"NetDeckInfiltration")', R)
        self.assertIn("return wrappedMethod(playerRef, targetRecordId, action, programItemID);", R)
        self.assertIn("wanted.m_wantedLevel = EPreventionHeatStage.Heat_5", R)
        self.assertIn("NetDeckTriggerNetWatchPursuit(npc)", R)

    def test_native_vehicle_actions_and_world_registry(self):
        self.assertIn("DeviceAction.NetDeckVehicleInfiltration:", Y)
        self.assertIn("$base: DeviceAction.VehicleForceBrakesClassHack", Y)
        self.assertIn("objectActionType: ObjectActionType.VehicleQuickHack", Y)
        self.assertIn("NetDeckVehicleActionInfiltration()", R)
        self.assertIn("VehicleComponentPS", R)
        self.assertIn("gamedataNewPerkType.Intelligence_Right_Milestone_1", R)
        self.assertIn("vehicle.IsHackable()", R)
        self.assertIn("PlayerQuickhackData(ItemID.None(), vehicleRecord, 5)", R)
        self.assertIn("netDeckLoadedVehicles: array<wref<VehicleObject>>", R)
        self.assertIn("public func NetDeckRegisterVehicle(", R)
        self.assertIn("public func NetDeckUnregisterVehicle(", R)
        # Global vehicle lifecycle hooks run during world streaming; not safe.
        self.assertNotIn("@wrapMethod(VehicleObject)", R)
        self.assertIn("netDeckPlayer.NetDeckRegisterVehicle(vehicle);", R)
        self.assertIn("this.NetDeckRegisterVehicle(initialVehicle);", R)
        self.assertIn("protected cb func OnGameAttached() -> Bool", R)
        self.assertIn("protected cb func OnDetach() -> Bool", R)
        self.assertIn("VehicleNetrunnerQuickhackType.ForceBrakes", R)
        self.assertIn("vehicle.ActivateNetrunnerQuickhack(", R)
        self.assertIn("this.netDeckLoadedVehicles[k]", R)
        self.assertIn("Vector4.Distance(this.netDeckWaveHosts[host], car.GetWorldPosition())", R)
        self.assertNotIn("TSFMV.Obj_Vehicle", R)

    def test_canto_enemy_action_not_ping_derived(self):
        # Clones Canto's attack-class targeting record, never Blackwall's
        # lethal gameplay effects. Real gameplay still requires manual tests.
        self.assertIn("$base: QuickHack.BaseBlackWallHack", Y)
        self.assertNotIn("$base: QuickHack.FreePingHack", Y)
        self.assertIn("QuickHack.NetDeckPuppetInfiltration:\n", Y)
        self.assertIn("startEffects: []", Y)
        self.assertIn("completionEffects: []", Y)
        self.assertIn("targetPrereqs: []", Y)
        self.assertIn("targetActivePrereqs: []", Y)
        self.assertNotIn("let pingable: Bool", R)
        self.assertNotIn("if !pingable", R)
        self.assertIn("let npc: ref<NPCPuppet> = owner as NPCPuppet;", R)
        self.assertIn("npc.IsQuest() || npc.IsDead() || npc.IsCrowd()", R)
        self.assertIn("ScriptedPuppet.IsPlayerCompanion(npc)", R)
        self.assertIn("npc.GetRecord().IsChild()", R)
        self.assertIn("NetDeckPuppetInfiltration extends PuppetAction", R)
        self.assertIn("OnNetDeckPuppetInfiltration", R)

    def test_loaded_npc_fallback_and_civilian_safety(self):
        # This only inspects the source. The actual hooks and chain must still
        # compile and be confirmed by a Cyberpunk 2077 in-game test.
        self.assertIn("netDeckLoadedPuppets: array<wref<NPCPuppet>>", R)
        self.assertIn("public func NetDeckRegisterPuppet(", R)
        self.assertIn("public func NetDeckUnregisterPuppet(", R)
        # Do not hook world-stream NPC lifecycle / SenseInitialize.
        self.assertNotIn("@wrapMethod(NPCPuppet)", R)
        self.assertIn("netDeckPlayer.NetDeckRegisterPuppet(npc);", R)
        self.assertIn("this.NetDeckRegisterPuppet(initialPuppet)", R)
        self.assertIn("let initialPuppet: wref<NPCPuppet> = root as NPCPuppet;", R)
        self.assertIn("this.netDeckLoadedPuppets[p]", R)
        self.assertIn("this.NetDeckWaveValid(loadedNpc)", R)
        self.assertIn("Vector4.Distance(this.netDeckWaveHosts[hostNpc], loadedNpc.GetWorldPosition())", R)
        self.assertIn("this.netDeckWaveLinks[hostNpc] < 4", R)
        self.assertIn("this.netDeckWaveDepths[hostNpc] < 4", R)
        self.assertIn("return EnumInt(GameObject.GetAttitudeTowards(npc, this)) !=", R)
        self.assertIn("EnumInt(EAIAttitude.AIA_Friendly)", R)
        self.assertIn("selectedNpcHost", R)
        self.assertIn("this.netDeckLoadedPuppets[p]", R)
        self.assertIn("let pendingCandidate: ref<GameObject>", R)
        self.assertLess(R.index("let selectedNpcHost: Int32"), R.index("let pendingCandidate: ref<GameObject>"))
        self.assertEqual(R.count("protected cb func OnGameAttached() -> Bool"), 0)
        self.assertEqual(R.count("protected cb func OnDetach() -> Bool"), 0)

    def test_no_vehicle_attach_code_runs_at_map_load(self):
        self.assertNotIn("@wrapMethod(VehicleObject)", R)
        self.assertNotIn("player.NetDeckRegisterVehicle(this);", R)
        self.assertNotIn("player.NetDeckUnregisterVehicle(this.GetEntityID());", R)
        self.assertIn("netDeckPlayer.NetDeckRegisterVehicle(vehicle);", R)
        self.assertIn("this.NetDeckRegisterVehicle(initialVehicle);", R)

    def test_no_npc_attach_code_runs_at_map_load(self):
        self.assertNotIn("@wrapMethod(NPCPuppet)", R)
        self.assertNotIn("player.NetDeckRegisterPuppet(this);", R)
        self.assertNotIn("player.NetDeckUnregisterPuppet(this.GetEntityID());", R)
        self.assertIn("netDeckPlayer.NetDeckRegisterPuppet(npc);", R)
        self.assertIn("protected cb func OnNetDeckPuppetInfiltration(", R)
        self.assertIn("GetTargetParts(\n      this, query, parts", R)
        self.assertIn("BaseStatusEffect.CommsNoiseLevel4", R)
        self.assertIn("UI_HUDProgressBar", R)

    def test_reward_and_feedback(self):
        self.assertIn("HackingRewardNotificationEvent", R)
        self.assertIn('loc.GetText("NetDeckDevicesAffected")', R)
        self.assertIn('loc.GetText("NetDeckTargetsAffected")', R)
        self.assertIn('loc.GetText("NetDeckEddiesStolen")', R)
        self.assertIn('loc.GetText("NetDeckStolenTargets")', R)
        self.assertIn("IntToString(deviceCount)", R)
        self.assertIn("this.netDeckWaveResult.devices", R)
        self.assertIn("NetDeckSiphon(", R)
        self.assertIn("Items.money", R)
        self.assertIn("if result.eddies >= 1500", R)
        self.assertIn("let barrier: ref<RoadBlockControllerPS>", R)
        self.assertIn("this.QueuePSEvent(barrier, toggle)", R)

    def test_networm_takeover_rules(self):
        self.assertIn("NetDeckBurnoutNearbyNetrunners(", R)
        self.assertIn("BaseStatusEffect.BrainMeltLevel4", R)
        self.assertIn("target.IsNetrunnerPuppet()", R)
        self.assertIn("EnumInt(GameObject.GetAttitudeTowards(target, player)) == EnumInt(EAIAttitude.AIA_Hostile)", R)
        self.assertIn("EnumInt(GameObject.GetAttitudeTowards(npc, player)) == EnumInt(EAIAttitude.AIA_Hostile)", R)
        self.assertNotRegex(R, r"GetAttitudeTowards\\([^\\n]+==\\s*EAIAttitude")
        self.assertIn("NetDeckDisruptNpcVicinity(", R)
        self.assertIn("BaseStatusEffect.CommsNoiseLevel4", R)
        self.assertNotIn("BaseStatusEffect.WeaponMalfunctionLvl2", R)
        self.assertNotIn("BaseStatusEffect.WeaponMalfunctionLvl4", R)
        self.assertIn("vehicle.ForceBrakesFor(4.0)", R)
        self.assertIn("door.ActionSetOpened()", R)
        self.assertIn("door.ActionToggleLock()", R)
        self.assertIn("door.IsMarkedAsQuest()", R)
        self.assertIn("NetDeckIsNetWatch(", R)
        self.assertIn("gamedataAffiliation.NetWatch", R)
        self.assertIn("wanted.m_wantedLevel = EPreventionHeatStage.Heat_5", R)
        self.assertIn("NPCPuppet.RevealPlayerPositionIfNeeded(", R)
        self.assertIn("NetDeckGiveDataComponents(", R)
        self.assertIn("Items.LegendaryMaterial1", R)
        self.assertIn("Items.QuickHackLegendaryMaterial1", R)
        self.assertNotIn("RPGManager.AwardXP(", R)
        self.assertNotIn("AddExperience(", R)

    def test_19_ui_languages_and_fallback(self):
        import re
        locale_ids = set(re.findall(r'if Equals\(language, n"([a-z]{2}-[a-z]{2})"\)', L))
        self.assertEqual(locale_ids, {
            "en-us", "ar-ar", "zh-cn", "zh-tw", "cz-cz",
            "fr-fr", "de-de", "hu-hu", "it-it", "jp-jp",
            "kr-kr", "pl-pl", "pt-br", "ru-ru", "es-es",
            "es-mx", "th-th", "tr-tr", "ua-ua"
        })
        self.assertEqual(L.count('this.Text("NetDeckSpreadComplete"'), 19)
        self.assertEqual(L.count('this.Text("NetDeckSpreadEmpty"'), 19)
        self.assertEqual(L.count('this.Text("NetDeckDevicesAffected"'), 19)
        self.assertEqual(L.count('this.Text("NetDeckEddiesStolen"'), 19)
        self.assertEqual(L.count('this.Text("NetDeckStolenTargets"'), 19)
        self.assertEqual(L.count('this.Text("NetDeckWaveProgress"'), 19)
        self.assertEqual(L.count('this.Text("NetDeckTargetsAffected"'), 19)

    def test_no_uncooked_placeholders(self):
        self.assertFalse((ROOT / "source").exists())
        self.assertIn('l"NetDeckInfiltrationName"', Y)
        self.assertNotIn("$base: DeviceAction.PingDevice", Y)
        self.assertNotIn("$base: Interactions.PingHack", Y)
        self.assertIn("$base: DeviceAction.ToggleStateClassHack", Y)
        self.assertIn("$base: Interactions.TakeControlCameraHack", Y)
        self.assertIn("m1nsook\\netdeck\\ui\\netdeck_icons.inkatlas", Y)
        self.assertIn("m1nsook\\netdeck\\ui\\networm_icons.inkatlas", Y)
        self.assertIn("captionIcon: ChoiceCaptionParts.NetDeckNetwormIcon", Y)

    def test_independent_native_labels(self):
        for key in ("NetDeckDeckName", "NetDeckDeckDescription",
                    "NetDeckInfiltrationName", "NetDeckInfiltrationDescription"):
            self.assertIn('l"' + key + '"', Y)
            self.assertIn('this.Text("' + key + '"', L)
        self.assertIn("extends ModLocalizationProvider", L)
        self.assertIn('return n"en-us"', L)
        self.assertNotIn("PingDevice", L)
        self.assertNotIn("$base: DeviceAction.PingDevice", Y)

if __name__ == "__main__":
    unittest.main(verbosity=2)
