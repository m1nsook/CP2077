# NΣT — Iconic Cyberdeck

**Status: experimental developer prototype. NOT a validated installable release.**

The goal is a standalone NetWatch Netdriver-derived iconic cyberdeck with its own built-in quickhack, **Infiltration**. It shares no files or hooks with the published [Camera Killer](../../Camera_Killer) mod and does not require Camera Killer to be installed.

## Cyberdeck specifications

| Property | Value |
| --- | --- |
| Tier | 5++ (Iconic) |
| Cyberware Capacity | 5 |
| RAM Units | 15 |
| Quickhack Slots | 4 |
| Buffer Size | 4 |
| Built-in quickhack | Infiltration |
| Source item | `Items.AdvancedNetwatchNetdriverMKLegendaryPlusPlus` |
| New item TweakDBID | `Items.NESTCyberdeck` |

The RAM and buffer stats are defined in the deck's equip package. The original Netdriver equip package is replaced instead of stacking 15 RAM on top of its original 13.

## Planned gameplay

V scans **one device with a real network link** and selects **Infiltration** in the usual scanner quickhack list. The game performs its normal quickhack action. The mod traverses that device's access point or surveillance-system descendants, never searches an arbitrary radius.

- **Cameras, turrets, laser mines:** game-owned `SetDeviceOFF` event.
- **Other non-quest, powered devices:** game-owned `QuickHackDistraction` event (6 seconds) as the fallback.
- **Quest-marked devices, disconnected devices and system masters:** skipped.
- **Robots, drones and mechs:** planned next; do not claim supported until their puppet-device links can be safely resolved and a non-lethal shutdown is verified.
- **No effects outside the selected network.**

The initial REDscript uses a native custom `ActionBool` action. Runtime dispatch and RAM/upload behavior **must be tested in Cyberpunk 2077 2.5**. It is not verified that every target can produce a distraction.

## All ripperdocs

The TweakXL stock patch appends `NEST.RipperdocStock` to the 26 known ripperdoc records, including Dogtown, with `Vendors.Always_Present`. It does not replace any existing item stock. Verify with an early-game vendor and a Dogtown vendor before publication.

## Development / installation

The gameplay source goes under:

- `r6/tweaks/NEST/NEST.yaml` — item, quickhack and 26 ripperdoc stock entries
- `r6/scripts/NEST/NEST.reds` — network resolution and native quickhack action

**Not ready for players:** the custom name/description are defined under `source/` in a WolvenKit JSON export and ArchiveXL source manifest. They must first be built into a proper `.archive` and packaged with the installable files. Until then the game may show missing localization keys. Similarly, the custom cyberdeck icon still needs an InkAtlas asset created with WolvenKit; the cloned Netdriver icon is a temporary placeholder.

## First in-game validation order

1. Verify REDscript compiles and TweakXL accepts the YAML.
2. Check `Game.AddToInventory("Items.NESTCyberdeck", 1)` in CET. Inspect 5 / 15 / 4 / 4 values.
3. Confirm stock at two unrelated ripperdocs, including Dogtown.
4. Equip NΣT: Infiltration should appear when scanning a network-linked camera, and disappear after removing the deck.
5. Cast Infiltration; verify a normal quickhack upload and actual OFF events on linked cameras/turrets/mines.
6. Confirm that unrelated networks, quest devices, and disabled targets remain untouched.
7. Add robots/drones and packed localization/icon **only after** the above works.

**No GitHub release should be published for this prototype.** Publish after the game test, art packaging, and robot behavior are complete.
