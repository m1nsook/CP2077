# Infiltration — NetWatch Σ MK.1 iconic cyberdeck

Cyberpunk 2077 2.5 development test. **v0.5.4 device-count feedback update is not REDscript-compiler verified yet.**
NetWatch Σ MK.1 is an iconic Tier 5++ cyberdeck with the built-in NETWORM quickhack.
The saved item ID remains `Items.NESTCyberdeck`; the directory and new
internal prefixes are `NetDeck`.

## User-confirmed baseline (v0.2.1)
* Capacity **5**, RAM **15**, **4** quickhack slots, **4** buffer.
* Existing Netdriver perks.
* Same-network cameras/turrets/mines shut down; unrelated network and quest devices unaffected.
* Night City and Dogtown ripperdoc stock.
* Camera Killer not required, and not modified.

## v0.5.4 target eligibility, takeover and feedback

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

NETWORM's network feedback remains a short native pulse plus a localized upload result.
**v0.5.4 adds a per-upload device counter** to that existing notification, e.g.
`NETWORM: NETWORK INFILTRATED | DEVICES: 4`. The label is translated in all
19 UI languages. Only device-controller actions accepted for dispatch through
the traversed network are counted; NPC Weapon Glitch, netrunner Synapse
Burnout and vehicle braking do **not** increase this count. The current
counter represents queued device actions, **not a separate game-engine
confirmation** that every action finished successfully. A NETWORM upload
that affects only an NPC may show `DEVICES: 0`. Empty-result feedback
is unchanged.

### v0.5.3 takeover rules (experimental)
- **Access-point scanner quickhacking removed.** Vanilla access-point breach interactions remain unchanged; regular device NETWORM can still traverse connected controllers.
- **Device-origin NETWORM:** cameras/turrets/mines and other compatible network electronics remain targets. Enemy **netrunners within 25 m of the infected device** receive a Tier 5 Synapse Burnout status effect. Ordinary NPCs are never damaged by this device-origin path.
- **NPC-origin NETWORM:** non-damaging Weapon Glitch (lower-tier Weapon Malfunction status) on hostile/NCPD/NetWatch targets; attempts Emergency Brake on eligible nearby vehicles within 25 m. No direct lethal action on the NPC, and the linked electronic network can still be affected.
- **NetWatch NPC upload:** issues the native five-star pursuit request and starts the game's position-reveal/trace path immediately. This relies on vanilla police/story constraints and may not equal an already completed trace in every state.
- **Doors/windows:** opens connected non-quest/non-sealed/non-disabled/non-elevator door/window controllers using native toggle-lock and open actions. Scripted quest barriers remain untouched.
- **Data extraction:** a successful upload gives 1–4 randomly selected item-crafting or quickhack-crafting components; the game's reward notification remains visible.
- **XP:** this mod explicitly awards **no XP**. However, engine-originated combat/skill XP (especially after Synapse Burnout kills) has **not** yet been suppressed and must be checked in-game. Do not claim XP-free until confirmed.

### What has and has not been tested
- **Confirmed in-game:** NetWatch Σ MK.1 cyberdeck icon and NETWORM icon display correctly when the user packs their private original-resolution artwork.
- **Compiler:** v0.5.1 failed due to invalid casts and a wrong method signature; v0.5.2 source corrected the reported signatures, but a subsequent successful game build has not been reported.
- **Door screenshot:** a user screenshot shows NETWORM's network-infiltrated message while a nearby door still reads `Open [AUTHORIZATION NOT FOUND]`. This does not prove the door is a linked, eligible network descendant; door unlock remains unverified. The counter does not fix or bypass door authorization.
- **New in v0.5.3, not verified:** remote door opening, vehicle braking, nonlethal Weapon Glitch, netrunner burnout, NetWatch full pursuit and trace, loot, and XP suppression.
- **Safety:** no quest-marked devices, sealed doors, quest/scene vehicles, or quest NPCs should be forcibly modified. Test with a backed-up save.

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

## Focused v0.5.3 game test
1. Confirm REDscript compiles. Send `r6/logs/redscript_rCURRENT.log` if not.
2. Confirm NETWORM remains separate from normal Ping on standalone devices and regular NPCs.
3. Scan a vanilla access point: the failed experimental NETWORM entry **must not appear**.
4. Upload to a device near an enemy netrunner: verify Synapse Burnout hits **only netrunners**, not other NPCs; test >25 m too.
5. Upload to a hostile NPC/NCPD/NetWatch: weapon glitches without damage; a nearby non-quest vehicle brakes, an unrelated vehicle does not.
6. Upload to a NetWatch NPC: check five-star pursuit and whether reveal/trace truly finishes instantly.
7. Test a regular network-connected closed door/window vs a quest/sealed door.
8. Verify occasional crafting/quickhack component rewards on successful upload and check both Netrunner XP and kill XP changes.
9. Confirm no unrelated-network shutdown, vanilla Ping regression, or private atlas changes.


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
