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

    def test_vendor_coverage(self):
        self.assertEqual(len(set(re.findall(r"^Vendors\.(\w+)\.itemStock:", Y, re.M))), VENDORS)
        self.assertIn("Vendors.AdvancedCollectiveRipperDocDogtown.itemStock:", Y)
        self.assertIn("Vendors.Always_Present", Y)

    def test_native_action_gate(self):
        for s in ["module NetDeck", "class NetDeckInfiltration extends ActionBool",
                  "FinalizeGetQuickHackActions(", "GetPlayerQuickHackListWithQuality(",
                  "PlayerQuickhackData(ItemID.None(), record, 5)",
                  "DeviceAction.NetDeckInfiltration",
                  "protected cb func OnNetDeckInfiltration"]:
            self.assertIn(s, R)
        self.assertNotIn("public func GetRemoteActions(", R)
        self.assertNotIn("member as PuppetDeviceLinkPS", R)
        self.assertIn("outActions: script_ref<array<ref<DeviceAction>>>", R)
        self.assertIn("const context: script_ref<GetActionsContext>", R)
        self.assertIn("protected final func FinalizeGetQuickHackActions(", R)
        self.assertIn("wrappedMethod(outActions, context);", R)
        self.assertIn("Items.NESTCyberdeck", R)

    def test_network_safety(self):
        for s in ["this.GetAccessPoints()", "GetAllDescendants(members)",
                  "ArrayContains(seen, member)", "member.IsMarkedAsQuest()",
                  "SurveillanceCameraControllerPS", "SecurityTurretControllerPS",
                  "ExplosiveTriggerDeviceControllerPS", "QuickHackDistraction",
                  "member.GetQuickHackActions(actions, context)"]:
            self.assertIn(s, R)
        self.assertNotIn("SystemCollapse", R)

    def test_no_uncooked_placeholders(self):
        self.assertFalse((ROOT / "source").exists())
        self.assertIn('l"NetDeckInfiltrationName"', Y)
        self.assertNotIn("$base: DeviceAction.PingDevice", Y)
        self.assertNotIn("$base: Interactions.PingHack", Y)
        self.assertIn("$base: DeviceAction.ToggleStateClassHack", Y)
        self.assertIn("$base: Interactions.TakeControlCameraHack", Y)

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
