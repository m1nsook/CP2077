# Infiltration — NetWatch Σ MK.1 iconic cyberdeck

Cyberpunk 2077 2.5 development test. **v0.5.2 is not REDscript-compiler verified yet.**
NetWatch Σ MK.1 is an iconic Tier 5++ cyberdeck with the built-in NETWORM quickhack.
The saved item ID remains `Items.NESTCyberdeck`; the directory and new
internal prefixes are `NetDeck`.

## User-confirmed baseline (v0.2.1)
* Capacity **5**, RAM **15**, **4** quickhack slots, **4** buffer.
* Existing Netdriver perks.
* Same-network cameras/turrets/mines shut down; unrelated network and quest devices unaffected.
* Night City and Dogtown ripperdoc stock.
* Camera Killer not required, and not modified.

## v0.5.2 target eligibility and feedback

**NETWORM should be offered on anything vanilla Ping can target.** A
camera, access point or surveillance system is no longer required.

* Native quickhackable **devices**, even without a network master, offer
  NETWORM. A standalone device receives only a native supported shutdown
  or distraction, never cross-network effects.
* **NPCs** with a Ping action in the native puppet scanner list now get an
  independent PuppetQuickHack form of NETWORM. On upload, the linked
  PuppetDeviceLinkPS is traversed through its real access point(s), then
  connected security devices are disabled/distracted.
* NPCs without a real device network link can receive the upload, but no
  surrounding devices are affected; the mod never guesses nearby connections.
* Quest-critical entities are skipped, and only actual linked network
  descendants are affected.
* Robots, drones and mechs are **not shut down yet** as NPCs, even though a
  pingable robotic puppet could be an entry point. That needs a separate
  safe puppet shutdown implementation.

Normal **Ping must remain separate and functional**. NETWORM uses a
unique action name and a dedicated device and puppet action record, not
`DeviceAction.PingDevice`. The actual quickhack UI is localized using
Codeware's provider (19 UI languages with English fallback). Cyberdeck and NETWORM icon paths are defined in TweakXL, but their separately cooked private atlas and texture files are **not** in this repository.

The 26 ripperdoc records and NetWatch Σ MK.1 gameplay stat packages are **unchanged**.

NETWORM's first-pass feedback uses a short native network pulse and a localized upload result. Access-point-only NETWORM scanning remains **experimental**. The v0.5.2 source fixes invalid `Device` casts and uses the correct `GetRemoteActions(out outActions: array<ref<DeviceAction>>, const context: script_ref<GetActionsContext>)` wrapper signature, but a successful game build has not been reported yet.

## Install
Requires RED4ext, redscript, TweakXL, Codeware and Cyberpunk 2077 2.5.

**Public source only:** GitHub ZIPs do **not** include original PNGs, `.inkatlas`, `.xbm`, `.archive`, or the WolvenKit project. The original user's working icons remain local. If testing icons, use your separately packed private WolvenKit archive.
Back up a save, close the game and remove old `r6/scripts/NEST` and
`r6/tweaks/NEST` directories (if present). Extract the test ZIP to the
game root. Replace existing `r6/scripts/NetDeck` and `r6/tweaks/NetDeck`
files, leaving Camera Killer and other mods untouched.

CET item command:
```lua
Game.AddToInventory("Items.NESTCyberdeck", 1)
```

## Focused live test for this build
1. Confirm no REDscript compiler/TweakXL error (previous issue: incorrect `GetRemoteActions` signature).
2. Scan a standalone pingable device **without a camera**. NETWORM must appear.
3. Scan a pingable hostile NPC; NETWORM must appear alongside normal Ping.
4. With a real access point link (camera or no camera), upload NETWORM and
   confirm devices on that network shut down, and unrelated networks do not.
5. Confirm **normal Ping still works** with NetWatch Σ MK.1 and a vanilla cyberdeck.
6. Scan an access point with this deck and another deck. It must offer only NETWORM on NetWatch Σ MK.1; no extra hacks on other decks.
7. Check whether the network pulse and localized success message appear after upload.

Previous gameplay results do not establish that this new NPC scanner hook,
standalone targeting, access-point actions, feedback or localization work.
Keep PR #36 in draft until verified.

## Naming and compatibility

**Mod:** Infiltration (repository project directory `NetDeck`).
**Cyberdeck shown in the game:** `NetWatch Σ MK.1`.
**Quickhack shown in the scanner:** `NETWORM`.

The technical action IDs `NetDeckInfiltration`, `DeviceAction.NetDeckInfiltration`,
and `QuickHack.NetDeckPuppetInfiltration` have **not** been renamed: their
user-facing labels come from Codeware localization. The existing item ID
`Items.NESTCyberdeck` also stays the same to avoid breaking saved equipment.
The rename does not alter cyberware stats, perks, network traversal or Ping.
