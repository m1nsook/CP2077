# Infiltration — NetWatch Σ MK.1 / NETWORM

Cyberpunk 2077 2.50 · M1 Arc integration · **1.0.0 release candidate**

The original NetWatch Σ MK.1 cyberdeck uses its existing save-compatible item ID `Items.NESTCyberdeck`. NETWORM is a built-in quickhack for computers, connected devices, eligible hostile NPCs and (with Carhacker) supported vehicles. Does not replace vanilla Ping.

## Requirements

- Cyberpunk 2077 2.50 and **Phantom Liberty**
- RED4ext and REDscript
- TweakXL and Codeware
- [M1 Arc 1.0.0+](https://github.com/m1nsook/m1arc)

**CET is not required.** Remove any previously installed `bin/x64/plugins/cyber_engine_tweaks/mods/NetDeckSettings` and `NetDeckDiagnostics` folders from older test builds. They are not included in the release.

## Installation

Extract the release ZIP into your Cyberpunk 2077 game directory. The ZIP contains `r6/` and `red4ext/` directories. Restart the game and look under **M1 Arc → Infiltration**. The TweakXL vendor records add the deck to 26 known ripperdoc lists without replacing other stock.

If the deck does not appear at a ripperdoc, load a save, open **M1 Arc → Infiltration → Cyberdeck**, then press **Add cyberdeck to V's inventory**. Each press gives **one** NetWatch Σ MK.1. The item is not automatically equipped; visit a ripperdoc to install it. Existing save references remain `Items.NESTCyberdeck`.

## M1 Arc settings

Seven live, persistent controls, identical in range/default to the previous CET test menu:

| Setting | Range | Default |
| --- | --- | --- |
| Upload HUD duration | 0.7–4.5 s | 2 s |
| Pre-scan radius | 30–180 m | 105 m |
| Relay hop radius | 15–130 m | 75 m |
| Infection step delay | 0.10–2.50 s | 0.25 s |
| Maximum steps | 4–150 | 100 |
| Maximum affected targets | 8–160 | 120 |
| Relay fanout | 1–16 | 10 |

Use **Club takeover**, **Balanced** or **Performance safe** preset buttons to change all seven controls together. Other mods' M1 Arc settings are left untouched. Changes apply to the loaded player within about half a second, including after loading another save.

## Features

- NETWORM's timed network propagation, shared device network effects and nonlethal communications disruption.
- A standalone virtual quickhack while the deck is equipped; ordinary Ping and physical quickhack slots remain separate.
- NetWatch retaliation, small capped €$ siphon and component rewards.
- NETWORM-only native upload progress and existing blue icon references.
- NPC eligibility protections for quest characters, children, companions and civilian crowds.

**Vehicle safety:** Automatic vehicle infection and forced braking remain disabled following the November 19 crash-isolation patch. A manually hacked eligible vehicle can act as a NETWORM entry point. Do not restore `VehicleObject.ActivateNetrunnerQuickhack` in the propagation path without specific engine validation.

## Icon artwork

The public source repository and automatic release packager **do not include the author's privately cooked** `NetDeck.archive`, `.inkatlas`, `.xbm` or WolvenKit project. The TweakXL records retain the atlas paths. The private archive must be installed separately for custom art to appear. Publishing a fully self-contained Nexus package requires adding the authorized cooked archive to the release outside this public source workflow.

## Verification

Core NETWORM behavior was tested by the mod author on the previous build. The **M1 Arc integration is new** and needs in-game verification of setting application, the preset buttons and one-click inventory grant. CI checks source invariants, release ZIP contents and REDscript parser syntax; semantic game compilation requires a private 2.50 game script bundle.

**Author:** 민숙 MISO @m1nsook · MIT (repository LICENSE).
