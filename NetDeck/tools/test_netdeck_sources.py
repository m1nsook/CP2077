#!/usr/bin/env python3
"""Static source invariants; intentionally NOT a redscript compilation test."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
R = (ROOT / "r6/scripts/NetDeck/NetDeck.reds").read_text("utf-8")
Y = (ROOT / "r6/tweaks/NetDeck/NetDeck.yaml").read_text("utf-8")
L = (ROOT / "r6/scripts/NetDeck/Localization.reds").read_text("utf-8")
VENDORS = 26

class InfiltrationSourceTests(unittest.TestCase):
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
        self.assertIn("@wrapMethod(ScriptableDeviceComponentPS)\npublic func GetRemoteActions(", R)
        self.assertIn("out outActions: array<ref<DeviceAction>>", R)
        self.assertIn("const context: script_ref<GetActionsContext>", R)
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

    def test_access_points_and_feedback_are_experimental(self):
        self.assertIn("@wrapMethod(AccessPointControllerPS)", R)
        self.assertIn("CanCreateAnyQuickHackActions()", R)
        self.assertIn("ArrayClear(outActions);", R)
        self.assertIn("NetDeckPulseNetwork(", R)
        self.assertIn("NetDeckNotify(", R)
        self.assertIn("HackingRewardNotificationEvent", R)

    def test_19_ui_languages_and_fallback(self):
        import re
        locale_ids = set(re.findall(r'if Equals\\(language, n"([a-z]{2}-[a-z]{2})"\\)', L))
        self.assertEqual(locale_ids, {
            "en-us", "ar-ar", "zh-cn", "zh-tw", "cz-cz",
            "fr-fr", "de-de", "hu-hu", "it-it", "jp-jp",
            "kr-kr", "pl-pl", "pt-br", "ru-ru", "es-es",
            "es-mx", "th-th", "tr-tr", "ua-ua"
        })
        self.assertEqual(L.count('this.Text("NetDeckSpreadComplete"'), 19)
        self.assertEqual(L.count('this.Text("NetDeckSpreadEmpty"'), 19)

    def test_no_uncooked_placeholders(self):
        self.assertFalse((ROOT / "source").exists())
        self.assertIn('l"NetDeckInfiltrationName"', Y)
        self.assertNotIn("$base: DeviceAction.PingDevice", Y)
        self.assertNotIn("$base: Interactions.PingHack", Y)
        self.assertIn("$base: DeviceAction.ToggleStateClassHack", Y)
        self.assertIn("$base: Interactions.TakeControlCameraHack", Y)
        self.assertIn("m1nsook\\\\netdeck\\\\ui\\\\netdeck_icons.inkatlas", Y)
        self.assertIn("m1nsook\\\\netdeck\\\\ui\\\\networm_icons.inkatlas", Y)
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
