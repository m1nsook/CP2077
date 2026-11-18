# Infiltration — NΣT iconic cyberdeck

Cyberpunk 2077 2.5 development test. **Not verified in game for v0.4.**
NΣT is an iconic Tier 5++ NetWatch Netdriver Mk.2 cyberdeck.
The saved item ID remains `Items.NESTCyberdeck`; the directory and new
internal prefixes are `NetDeck`.

## User-confirmed baseline (v0.2.1)
* Capacity **5**, RAM **15**, **4** quickhack slots, **4** buffer.
* Existing Netdriver perks.
* Same-network cameras/turrets/mines shut down; unrelated network and quest devices unaffected.
* Night City and Dogtown ripperdoc stock.
* Camera Killer not required, and not modified.

## v0.4 target eligibility

**Infiltration should be offered on anything vanilla Ping can target.** A
camera, access point or surveillance system is no longer required.

* Native quickhackable **devices**, even without a network master, offer
  Infiltration. A standalone device receives only a native supported shutdown
  or distraction, never cross-network effects.
* **NPCs** with a Ping action in the native puppet scanner list now get an
  independent PuppetQuickHack form of Infiltration. On upload, the linked
  PuppetDeviceLinkPS is traversed through its real access point(s), then
  connected security devices are disabled/distracted.
* NPCs without a real device network link can receive the upload, but no
  surrounding devices are affected; the mod never guesses nearby connections.
* Quest-critical entities are skipped, and only actual linked network
  descendants are affected.
* Robots, drones and mechs are **not shut down yet** as NPCs, even though a
  pingable robotic puppet could be an entry point. That needs a separate
  safe puppet shutdown implementation.

Normal **Ping must remain separate and functional**. Infiltration uses a
unique action name and a dedicated device and puppet action record, not
`DeviceAction.PingDevice`. The actual quickhack UI is localized using
Codeware's provider (English fallback). The icon is still vanilla.

The 26 ripperdoc records and NΣT gameplay stat packages are **unchanged**.

## Install
Requires RED4ext, redscript, TweakXL, Codeware and Cyberpunk 2077 2.5.
Back up a save, close the game and remove old `r6/scripts/NEST` and
`r6/tweaks/NEST` directories (if present). Extract the test ZIP to the
game root. Replace existing `r6/scripts/NetDeck` and `r6/tweaks/NetDeck`
files, leaving Camera Killer and other mods untouched.

CET item command:
```lua
Game.AddToInventory("Items.NESTCyberdeck", 1)
```

## Focused live test for this build
1. Confirm no REDscript compiler/TweakXL error.
2. Scan a standalone pingable device **without a camera**. Infiltration must appear.
3. Scan a pingable hostile NPC; Infiltration must appear alongside normal Ping.
4. With a real access point link (camera or no camera), upload Infiltration and
   confirm devices on that network shut down, and unrelated networks do not.
5. Confirm **normal Ping still works** with NΣT and a vanilla cyberdeck.

Previous gameplay results do not establish that this new NPC scanner hook,
standalone targeting, localization or the v0.3 Ping regression fix work.
Keep PR #36 in draft until verified.
