#!/usr/bin/env python3
"""Focused source-level tests for the NΣT prototype (not REDscript compilation)."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "r6/scripts/NEST/NEST.reds"
TWEAKS = ROOT / "r6/tweaks/NEST/NEST.yaml"
STRINGS = ROOT / "source/localization/en-us.json"

# Independent vendor records used by actual Night City and Dogtown ripperdocs.
KNOWN_VENDORS = {
    "AdvancedCollectiveRipperDoc", "AdvancedCollectiveRipperDocDogtown",
    "AdvancedCollectiveRipperDocGeneral", "AdvancedCollectiveRipperDocVictor",
    "AdvancedOSAndSkeletalRipperDoc", "AdvancedSkeletalRipperDoc",
    "bls_ina_se1_ripperdoc_01", "bls_ina_se1_ripperdoc_02",
    "cz_monument_ripperdoc_anderson", "cz_monument_ripperdoc_farida",
    "cz_stadium_ripperdoc_01", "EarlyGameRipperdoc", "EndGameRipperdoc",
    "hey_spr_ripperdoc_01", "MidGameRipperdoc", "pac_wwd_ripperdoc_01",
    "Ripperdoc_HUB", "std_arr_ripperdoc_01", "std_rcr_ripperdoc_01",
    "wat_kab_ripperdoc_01", "wat_kab_ripperdoc_02", "wat_kab_ripperdoc_03",
    "wat_lch_ripperdoc_01", "wat_nid_ripperdoc_01", "wbr_jpn_ripperdoc_01",
    "wbr_jpn_ripperdoc_02",
}

class NESTSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reds = SCRIPTS.read_text("utf-8")
        cls.yaml = TWEAKS.read_text("utf-8")
        cls.locale = json.loads(STRINGS.read_text("utf-8"))

    def test_exact_cyberdeck_stats(self):
        for value in ("Items.NESTCyberdeck:", "Items.CyberdeckBlueprint4Slots",
                      "Variants.Humanity5Cost", "NEST.RAM15Buffer4",
                      "statType: BaseStats.Memory", "value: 15",
                      "statType: BaseStats.BufferSize", "value: 4"):
            self.assertIn(value, self.yaml)
        self.assertNotIn("Items.AdvancedNetwatchNetdriverMKLegendaryPlusPlus_inline1", self.yaml)

    def test_every_vendor_record(self):
        found = set(re.findall(r"^Vendors\.(\w+)\.itemStock:", self.yaml, re.M))
        self.assertEqual(found, KNOWN_VENDORS)
        self.assertEqual(self.yaml.count("!append-once NEST.RipperdocStock"), 1)
        self.assertIn("Vendors.Always_Present", self.yaml)

    def test_equip_gate_and_real_network_links(self):
        self.assertIn("GetItemInEquipSlot", self.reds)
        self.assertIn("Items.NESTCyberdeck", self.reds)
        self.assertIn("this.GetAccessPoints()", self.reds)
        self.assertIn("master.GetAllDescendants(members)", self.reds)
        self.assertNotIn("GetTargetParts(", self.reds)
        self.assertNotIn("LogChannel(", self.reds)

    def test_action_event_name_matches_dispatch_type(self):
        self.assertIn("public class NESTInfiltration extends ActionBool", self.reds)
        self.assertIn("OnNESTInfiltration(evt: ref<NESTInfiltration>)", self.reds)
        self.assertIn("DeviceAction.NESTInfiltration", self.yaml)
        self.assertIn("actionName: NESTInfiltration", self.yaml)

    def test_exclusions_and_fallback(self):
        self.assertIn("this.IsMarkedAsQuest()", self.reds)
        self.assertIn("member.IsMarkedAsQuest()", self.reds)
        self.assertIn("ExplosiveTriggerDeviceControllerPS", self.reds)
        self.assertIn("SecurityTurretControllerPS", self.reds)
        self.assertIn("SurveillanceCameraControllerPS", self.reds)
        self.assertIn("ActionQuickHackDistraction()", self.reds)
        self.assertNotIn("SystemCollapse", self.reds)

    def test_localization_keys(self):
        entries = self.locale["Data"]["RootChunk"]["root"]["Data"]["entries"]
        keys = {entry["secondaryKey"] for entry in entries}
        self.assertEqual(keys, {"NESTDeckName", "NESTDeckDescription",
                                "NESTInfiltrationName", "NESTInfiltrationDescription"})
        for key in keys:
            self.assertIn('l"' + key + '"', self.yaml)

if __name__ == "__main__":
    unittest.main(verbosity=2)
