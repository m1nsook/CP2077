#!/usr/bin/env python3
"""Regression checks for Infiltration's native M1 Arc release candidate."""
from pathlib import Path
from collections import Counter
import json,re,unittest

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"r6/scripts/NetDeck/NetDeck.reds").read_text("utf-8")
HUD=(ROOT/"r6/scripts/NetDeck/NetDeckHudSkin.reds").read_text("utf-8")
BRIDGE=(ROOT/"r6/scripts/NetDeck/NetDeckM1Arc.reds").read_text("utf-8")
LOC=(ROOT/"r6/scripts/NetDeck/Localization.reds").read_text("utf-8")
YAML=(ROOT/"r6/tweaks/NetDeck/NetDeck.yaml").read_text("utf-8")
M=ROOT/"red4ext/plugins/m1arc/mods/m1nsook.infiltration"
MANIFEST=json.loads((M/"mod.json").read_text("utf-8"))
SETTINGS=json.loads((M/"settings.json").read_text("utf-8"))
IDs={s["id"] for s in SETTINGS}

class InfiltrationSourceTests(unittest.TestCase):
 def test_item_id_and_vendor_stock(self):
  self.assertIn("Items.NESTCyberdeck:",YAML)
  self.assertIn("$base: Items.AdvancedNetwatchNetdriverMKLegendaryPlusPlus",YAML)
  self.assertIn("NetDeck.RipperdocStock:",YAML)
  self.assertEqual(len(set(re.findall(r"^Vendors\.(\w+)\.itemStock:",YAML,re.M))),26)
  self.assertIn("Items.NESTCyberdeck",CORE)
  self.assertIn("NetDeck.RAM15Buffer4",YAML)

 def test_safe_gameplay_regressions(self):
  for snippet in ("NetDeckWaveStart(", "NetDeckWaveInfect(", "NetDeckPreviewNetwork(", "GetPlayerQuickHackListWithQuality(","NetDeckPuppetInfiltration","NetDeckVehicleInfiltration"):
   self.assertIn(snippet,CORE)
  self.assertNotIn("@wrapMethod(VehicleObject)",CORE)
  self.assertNotIn("@wrapMethod(NPCPuppet)",CORE)
  self.assertNotIn("VehicleNetrunnerQuickhackType.ForceBrakes",CORE)
  self.assertNotIn("vehicle.ActivateNetrunnerQuickhack(",CORE)
  self.assertNotIn("vehicle.ForceBrakesFor(",CORE)
  self.assertIn("wrappedMethod(outActions, context);",CORE)
  self.assertIn("this.NetDeckQueuePreviewCandidate(",CORE)
  self.assertIn("t\"Items.money\"",CORE)
  self.assertIn("QuickHack.BaseBlackWallHack",YAML)
  self.assertIn("Vendors.Always_Present",YAML)

 def test_hud_is_scoped_and_localized(self):
  self.assertIn("@wrapMethod(HUDProgressBarController)",HUD)
  self.assertIn("public final func UpdateProgressBarActive(active: Bool)",HUD)
  self.assertIn("wrappedMethod(active);",HUD)
  self.assertIn('StrBeginsWith(header, "NETWORM")',HUD)
  self.assertNotIn("@wrapMethod(HUDProgressBarController)",CORE)
  self.assertIn('this.Text("NetDeckInfiltrationName", "NETWORM")',LOC)
  self.assertEqual(LOC.count('this.Text("NetDeckUploadSingle"'),19)

 def test_manifest_schema_and_locales(self):
  self.assertEqual(MANIFEST["format"],1)
  self.assertEqual(MANIFEST["id"],M.name)
  self.assertEqual(MANIFEST["name"],"NetWatch Σ MK.1")
  self.assertEqual(MANIFEST["scope"],"Weapons")
  self.assertEqual(MANIFEST["requires"],{"m1arc":"1.0.0"})
  self.assertEqual(len(SETTINGS),8)
  self.assertEqual(len(IDs),8)
  for setting in SETTINGS:
   self.assertTrue(setting["label"] and setting["group"])
   if setting["kind"]=="action":
    self.assertNotIn("default",setting)
   else:
    self.assertLessEqual(setting["min"],setting["default"])
    self.assertLessEqual(setting["default"],setting["max"])
  for lang in ("en_US","pl_PL","ko_KR"):
   translated=json.loads((M/"language"/(lang+".json")).read_text("utf-8"))
   self.assertEqual(translated["name"],MANIFEST["name"])
   self.assertEqual(set(translated["settings"]),IDs)
   for row in translated["settings"].values():
    self.assertTrue(row["label"] and row["description"] and row["group"])

 def test_m1arc_settings_actions_and_grant(self):
  for setting in SETTINGS:
   if setting["kind"]!="action":
    native="M1Arc_GetInt" if setting["kind"]=="integer" else "M1Arc_GetFloat"
    self.assertIn(native+'("m1nsook.infiltration", "'+setting["id"]+'")',BRIDGE)
   else:
    self.assertIn('M1Arc_ConsumeAction("m1nsook.infiltration", "'+setting["id"]+'")',BRIDGE)
  self.assertIn('ts.GiveItemByTDBID(player, t"Items.NESTCyberdeck", 1);',BRIDGE)
  self.assertIn("player.NetDeckConfigure(",BRIDGE)
  self.assertIn("m_lastRevision",BRIDGE)
  self.assertIn("M1Arc_Revision()",BRIDGE)
  self.assertEqual({s["id"] for s in SETTINGS if s["kind"]=="action"},{"give_deck"})
  for old in ("preset_club","preset_balanced","preset_safe","ApplyPreset","M1Arc_SetFloat(","M1Arc_SetInt("):
   self.assertNotIn(old,BRIDGE)
  self.assertNotIn("registerForEvent(",BRIDGE)
  self.assertNotIn("OnGameAttached",BRIDGE)

 def test_cooked_icon_archive_is_packaged_unchanged(self):
  import hashlib
  archive=ROOT/"archive/pc/mod/NetDeck.archive"
  contents=archive.read_bytes()
  self.assertEqual(contents[:4],b"RDAR")
  self.assertEqual(len(contents),36864)
  self.assertEqual(hashlib.sha256(contents).hexdigest(),"eca64a62c8746fc797546182fb50f9ff46fd0741600a9d68dae6896a480ad0d7")

 def test_no_cet_files_or_gameplay_changes(self):
  self.assertFalse((ROOT/"bin/x64/plugins/cyber_engine_tweaks").exists())
  from pack_test import FILES
  self.assertTrue(all(not f.startswith("bin/") for f in FILES))
  self.assertEqual(len(FILES),11)
  self.assertIn("r6/scripts/NetDeck/NetDeckM1Arc.reds",FILES)
  self.assertIn("archive/pc/mod/NetDeck.archive",FILES)
  self.assertTrue(all((ROOT/f).exists() for f in FILES))

if __name__=="__main__":
 unittest.main(verbosity=2)
