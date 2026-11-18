# Infiltration — NetWatch Σ MK.1 / NETWORM

**Cyberpunk 2077 2.5 · v0.6.0 development source test · NOT compiler/gameplay verified.**

This folder contains the **public REDscript and TweakXL source only** for the
Infiltration mod and its built-in NETWORM quickhack. The cyberdeck display name
is **NetWatch Σ MK.1** (Tier 5++ iconic). The save-compatible item TweakDB ID
**must remain `Items.NESTCyberdeck`**. Internal source prefix: `NetDeck`.

## Confirmed baseline (player-tested)

- The custom cyberdeck and quickhack icons work in-game using the player's
  privately packed original-resolution CDPR assets.
- Cyberdeck: capacity 5, RAM 15, four quickhack slots, buffer 4, existing
  Netdriver perks; stocked at 26 Night City / Dogtown ripperdocs.
- Early same-network camera/turret/mine shutdown was tested in an earlier build.
- Normal Ping is a separate vanilla quickhack; never replace its action ID.

## v0.6.0 experimental features

### One-upload multi-hop propagation

One NETWORM upload now traverses a shared queue of **nearby hostile NPCs,
vehicles, and controllable devices**, plus the actual access-point descendants
of infected devices or NPCs. It can hop **18 meters per hop, four hops maximum**,
and processes **no more than 64 loaded entities per upload**. A visited list
prevents re-hacking one target twice within that upload. Because this version
deliberately supports radio/proximity jumps, it is no longer limited to the
single connected-device network of the original mod.

- **NPCs:** nearby hostile gang members, NCPD and NetWatch are individually
  affected by a **nonlethal Weapon Glitch**. One uploaded hack can reach several.
  Ordinary civilians are not deliberately damaged.
- **Vehicles:** eligible non-quest/non-player moving vehicles receive the
  native `ForceBrakesFor(4.0)` effect. Parked vehicles can relay a nearby hop.
- **Devices:** linked or nearby compatible cameras, turrets and mines are
  shut down; native device distraction may be dispatched on supported hardware.
  **RoadBlockControllerPS** blockades can be lowered via the game's native
  `ToggleBlockade` event; suitable connected, non-quest doors/windows may be
  unlocked and opened. Not every map object has a hackable game controller.
- **Enemy netrunners:** DEVICE-origin waves apply Synapse Burnout near each
  infected device (25 m); NPC-origin waves do not directly damage ordinary NPCs.
- **NetWatch retaliation:** when an infected NPC is affiliated with NetWatch,
  issue native five-star pursuit and a position-reveal attempt.
- **Access points:** no scanner quickhack action is added to access points;
  vanilla access-point breach mechanics remain intact.

### Stealth and feedback

NETWORM's own device and puppet quickhack records now use a dedicated
zero-awareness stat modifier group. CDPR's native virtual-program trace check
uses the quickhack's `awarenessCost`: intended result is untraceable NETWORM
for ordinary targets, **without changing any other quickhack's tracing**.
NetWatch's explicit five-star/reveal branch is retained. This behavior **still
needs an in-game test**; status-effect-triggered AI responses may differ.

Feedback uses the existing native `EXTRACTED DATA` notification and localized
text for **19 supported UI languages**. The message includes the count of
queued affected device actions, plus siphoned Eurodollars and the number of
sources if credits were collected. The counter measures queued actions, not
confirmed game-engine completion.

### Eurodollar siphon / pickpocket-style reward

Each newly visited eligible NPC or computer/terminal has a **65% chance** to
yield a small random amount of €$. A single upload is capped at **€$1,500**.
The game credits the player through its transaction system, but it is a
**virtual data-siphon gameplay reward**: it does not decrement a real target
inventory or bank account. Repeating uploads can generate more money; a
per-target persistent cooldown is not implemented yet.

Existing random crafting/quickhack component rewards on successful hacks are
preserved. NETWORM source explicitly does **not** award XP, but native
quickhack/kill/skill XP has **not** been suppressed; this remains an unmet
strict no-XP requirement until game-tested and fixed.

## Install and tests

Requires Cyberpunk 2077 2.5, RED4ext, REDscript, TweakXL and Codeware.
Back up your save and close the game first.

Source ZIPs from GitHub include only these game-root files:
- `r6/scripts/NetDeck/NetDeck.reds`
- `r6/scripts/NetDeck/Localization.reds`
- `r6/tweaks/NetDeck/NetDeck.yaml`

Extract to the game root and replace previous `NetDeck` script and tweak
files. Do not leave duplicate old `NEST` scripts/tweaks installed.
**Keep your existing privately cooked atlas .archive installed.** The GitHub
artifacts do **not** include PNG, `.inkatlas`, `.xbm`, `.archive`, raw/ArchiveXL
artwork or the WolvenKit project. Neither the assets nor the project should be
uploaded to this repo.

Test in this order using a backed-up save:

1. Game startup: check `r6/logs/redscript_rCURRENT.log` for compilation
   errors and `red4ext/plugins/TweakXL/TweakXL.log` for tweak failures.
2. Normal Ping still works; NETWORM appears only with NetWatch Σ MK.1 equipped.
3. One hack on a group of gang enemies: multiple nearby hostiles get Weapon
   Glitch; unrelated civilians are not damaged.
4. One hack on a moving vehicle: it brakes, and nearby NPCs/vehicles/devices
   can also be affected. Test stationary cars as potential relays.
5. One hack on a computer or nearby camera: connected and nearby devices
   respond; enemy netrunners near the infected DEVICE receive Synapse Burnout.
6. Try the road barrier shown in the test screenshot, plus a normal door and
   a quest-locked/sealed door. Quest barriers must stay protected.
7. Confirm ordinary gangs and NCPD do **not** trace NETWORM; NetWatch
   triggers the five-star pursuit and reveal attempt.
8. Confirm `EXTRACTED DATA` reports device count plus e.g. `€$250 / 3 TARGETS`
   when siphoning succeeds. Check actual €$ inventory balance.
9. Check earned XP after infiltration and any Synapse Burnout casualties.
10. Swap to a different deck; NETWORM should no longer be available.

**These are target behaviors, not confirmed results.** The v0.6.0 source has
not been compiled by the player's game yet. Static source tests in GitHub
Actions cannot verify REDscript signatures, availability of targeted world
objects or runtime effects. Keep PR #36 in draft until tested.

## Stable names and IDs

- Mod: **Infiltration**
- Cyberdeck: **NetWatch Σ MK.1**
- Quickhack: **NETWORM**
- Source directory: `NetDeck`
- Item: `Items.NESTCyberdeck` (preserved for saved equipment)
- Action IDs: `NetDeckInfiltration`,
  `DeviceAction.NetDeckInfiltration`, `QuickHack.NetDeckPuppetInfiltration`
  (all preserved for compatibility)
