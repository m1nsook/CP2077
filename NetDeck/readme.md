# Infiltration

**Status:** Developer test — REDscript and TweakXL runtime not verified in Cyberpunk 2077 2.5.

**Cyberware:** NΣT (NetWatch Netdriver Mk.2), an iconic Tier 5++ cyberdeck. This mod is independent of Camera Killer.

| Specification | Value |
| --- | --- |
| Capacity | 5 |
| RAM | 15 |
| Quickhack slots | 4 |
| Buffer | 4 |
| Legacy item ID (keep for saves) | `Items.NESTCyberdeck` |
| Source item | `Items.AdvancedNetwatchNetdriverMKLegendaryPlusPlus` |

## How it is intended to work

With NΣT installed, the scanner should offer the **built-in** Infiltration device quickhack on an on-line device linked to an actual access point or surveillance master. Its synthetic player quickhack entry does **not** occupy any of the four physical slots. The game should handle upload and RAM spending through the copied native quickhack record; the REDscript completion event then traverses **only the target's linked network**.

* Cameras, security turrets, and explosive trigger mines: request the game's device OFF action.
* Other ordinary, non-quest electronic devices: use an existing, active, device-provided **Distract Enemies** action where available.
* Masters, quest-marked devices, devices not on the selected network and powered-off targets: skipped.
* **Robots, drones, and mechs: still unsupported.** Their link representation and safe shutdown need verification using game assets. The current code deliberately does not attack arbitrary puppets.

### What changed from PR #36

* Folder, REDscript module, action names and TweakXL records renamed to `NetDeck`; the working item TDBID is preserved.
* Infiltration is now offered via `GetQuickHackActions` instead of ordinary `GetRemoteActions`.
* The player list is extended with a virtual built-in Infiltration action only while NΣT is installed, so the native scanner can match the command to the target action.
* Action completion uses a REDscript event callback.
* The custom action inherits vanilla Ping's device quickhack processing rather than the old Toggle State template.
* Removed the unusable ArchiveXL manifest and uncooked localization JSON. **As a result, native UI name and icon are not finished**: inherited game text/icon may be displayed. Real NΣT/Infiltration UI texts require a cooked WolvenKit localization archive. Do not mistake a test ZIP for a finished release.

### Vendor stock

The same 26 TweakXL ripperdoc vendor records are patched, including Dogtown. This has not been checked at every vendor.

## Install this test ZIP

Close the game. **Remove older `r6/scripts/NEST` and `r6/tweaks/NEST` folders** so previous wrappers are not loaded twice. Extract the test ZIP to the Cyberpunk 2077 game root. Requires RED4ext, redscript and TweakXL. Cyberware-EX is supported by the 0–3 system-replacement equip-slot check (not independently validated).

Add the deck with CET:
```lua
Game.AddToInventory("Items.NESTCyberdeck", 1)
```

If the game fails to load, check `r6/logs/redscript_rCURRENT.log` (or the installed redscript log) and `red4ext/plugins/TweakXL/TweakXL.log`.

## Testing and limitations

1. Confirm the deck still shows 5 / 15 / 4 / 4 and existing Netdriver perks.
2. Equip NΣT; scan a linked camera. Verify an Infiltration command is actually offered and uploads normally. In the test build, untranslated native UI text may appear instead.
3. Verify same-network cameras/turrets/mines go off and a second, unrelated network does not.
4. Verify quest-marked and unlinked devices are ignored; test with and without Camera Killer.
5. Verify stock at a Night City ripperdoc and in Dogtown.
6. Robots are **not** marked supported until a non-destructive native puppet/network integration is established.

This build has **not** passed a 2.5 game or REDscript compiler check. Static tests alone cannot establish that the action uploads, that RAM is spent or that network devices turn off. Do not publish as a finished mod.

### 2026-11-18 REDscript compiler correction

The previous NetDeck test failed to compile because the base-class `GetQuickHackActions` wrapper signature does not match the 2.5 method. The corrected hook uses the 2.5 `FinalizeGetQuickHackActions` signature with script references. An impossible `PuppetDeviceLinkPS` cast was also removed. Source correction only; this is not an in-game success claim.
