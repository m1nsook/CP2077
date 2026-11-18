# Infiltration — NetWatch Σ MK.1 / NETWORM

**Cyberpunk 2077 2.5 · v0.6.2 VEHICLE PATH TEST · NOT game-compiler verified.**

**Mod:** Infiltration. **Deck:** NetWatch Σ MK.1. **Quickhack:** NETWORM.
**Source folder/prefix:** NetDeck. **Save-compatible item:** `Items.NESTCyberdeck`.

## Why this rewrite exists

The player's in-game v0.6.0 test showed **no NPC-to-NPC or vehicle
propagation**, only a sometimes-successful €$ reward from the *one* target,
and ordinary quickhack tracing. The initial implementation incorrectly ran
`TargetingSystem.GetTargetParts` from arbitrary hacked devices/NPCs,
without using a reliable full-area query anchored on V. Pushing the source
code was NOT proof it functioned.

The user-supplied `HeatSpread Files(1).zip` contained:
- **HeatSpread_2025:** a CET mod adjusting vanilla Overheat's
  `spreadCount`, `bonusJumps`, `spreadDistance` in `QuickHack.BaseOverheatHack_inline3`.
  It does not implement a custom propagation loop.
- **Echo_EMP v0.2b:** a custom **REDscript delayed event** on `PlayerPuppet`,
  using `TargetingSet.Complete`, a full-area 3D search from V, infected
  host positions and a hop/fanout limit. This is the model for the new
  NETWORM tick scheduling; NETWORM retains its own nonlethal effects.

## v0.6.2: native vehicle path and propagation (unverified)

The game does not define `TSFMV.Obj_Vehicle`, and `Obj_Other`
is not a reliable way to discover traffic. Vehicles use a separate
`VehicleComponentPS.GetQuickHackActions` pipeline, which calls the
shared quickhack finalizer; normal vehicle hacks are unlocked by the
**Carhacker** Intelligence perk.

The NetDeck source now:
- Adds a distinct built-in `VehicleQuickHack` action,
  `DeviceAction.NetDeckVehicleInfiltration`, which shares the
  existing NETWORM actionName, icon and effects.
- Offers it only for an equipped NetWatch Σ MK.1 on eligible,
  hackable, non-player, non-quest vehicles with the Carhacker perk.
- Uses native `VehicleObject.OnGameAttached` / `OnDetach` events to
  maintain a bounded weak-reference registry of loaded vehicles.
  It checks vehicle world positions during each timed wave tick
  **separately from the TSFMV target search**.
- Uses native `VehicleObject.ActivateNetrunnerQuickhack(
  VehicleNetrunnerQuickhackType.ForceBrakes)` on moving vehicles,
  matching the in-game braking quickhack entry point.
- Allows parked eligible cars to relay the worm without forcing brakes.
- Keeps the existing four-hop, four-fanout, 40-target timed wave for
  NPCs and devices. A registry records loaded cars, but propagation
  has **not been demonstrated in-game yet**.

These additions do not affect the separate vanilla vehicle quickhacks,
NetWatch retaliation, normal Ping, or the user's private icon files.

## v0.6.1: timed-wave source implementation, not yet verified in-game

- One upload initializes a wave attached to the player and schedules a new
  step **every 0.45 seconds** (no simultaneous instant-burst loop).
- Each step checks a full-area targeting query **from V**, then selects one
  candidate within **25 m of a previously infected host**, up to **4 hops**,
  **4 outgoing links per host**, and **40 affected entities per upload**.
  The candidates must also be loaded within 55 m of V/root.
- Real access-point descendants are queued separately and processed one per
  tick, preserving device-network reach without needing to quickhack a
  vanilla access point directly.
- NPCs: hostile gangers/NCPD/NetWatch receive the nonlethal
  `WeaponMalfunctionLvl2` status one after another. Civilians, quest
  NPCs, children and companions are excluded from the wave.
- Vehicles: eligible moving non-quest vehicles get native
  `ForceBrakesFor(4.0)`; eligible parked vehicles can relay the wave.
- Devices: use existing native actions for security hardware,
  `RoadBlockControllerPS` barriers and eligible doors/windows; at most eight
  native network visual pulses per upload.
- Hostile netrunners: Synapse Burnout is applied only on device-origin waves
  near infected device controllers.
- Trace control: `QuickHackableHelper.WillHackRevealPlayer` is wrapped for
  **actionName = NetDeckInfiltration only**, returning false for ordinary
  NETWORM uploads. No other quickhacks' tracing is modified. NETWORM
  against a NetWatch NPC still issues its separate native five-star
  pursuit/position-reveal attempt.
- UI: progress notifications appear each **four newly infected targets**;
  final notification reports total targets, queued device actions, optional
  €$ stolen and the number of robbed sources. UI strings localized in all
  **19** supported interface languages.
- Money: 65% chance to grant a small virtual €$ siphon reward from each
  eligible NPC or computer/terminal, capped at €$1,500 per upload. This does
  NOT deduct money from a real NPC wallet.
- Random crafting component rewards retained. **Normal Netrunner XP is allowed** when using NETWORM; no XP suppression is required.

This version does not yet render an actual traveling beam between **NPCs**;
the visible changes are staggered native Weapon Glitch effects, device
network-link pulses and periodic localized notifications. A custom NPC-to-NPC
beam requires a separate verified VFX implementation.

## Source-only install

Requires Cyberpunk 2077 2.5, RED4ext, REDscript, TweakXL and Codeware.
Back up the save and close the game before installing.

Replace these three source files from the **GitHub source test**:
- `r6/scripts/NetDeck/NetDeck.reds`
- `r6/scripts/NetDeck/Localization.reds`
- `r6/tweaks/NetDeck/NetDeck.yaml`

Keep the user's privately packed `.archive` with the working icon art.
**GitHub does not include or publish the user's PNGs, .inkatlas, .xbm,
.archive, WolvenKit project or raw/ArchiveXL assets.**

## Live checklist — test before any release

1. **Compilation**: check `r6/logs/redscript_rCURRENT.log` and TweakXL
   logs. The new PlayerPuppet event, GetDeviceLink and targeted trace hook
   have NOT been compiler-verified on the user's installation.
2. **One gang upload**: watch 3–5 nearby enemies receive Weapon Glitch
   in order, spaced approximately 0.45 s apart, without retargeting.
3. **Progress feedback**: check `NETWORM: SPREADING 4` and later
   `TARGETS: <number>` in the finished notification.
4. **Carhacker perk:** confirm vanilla vehicle quickhacks and NETWORM are
   both offered on an eligible traffic vehicle. With a different deck,
   the new vehicle NETWORM must disappear.
5. **One vehicle upload:** confirm it triggers native Emergency Brake,
   then infects at least one other nearby car or NPC automatically.
   A parked car may relay but must not brake.
6. **One device upload**: confirm new device pulses, linked descendants
   and enemy-netrunner-only Synapse Burnout.
7. **Road barrier**: test ordinary blockades alongside a sealed/quest
   barrier, which must remain protected.
8. **Tracing**: ordinary gang/NCPD uploads should not trace. NETWORM
   against a true NetWatch NPC should produce a five-star response.
9. **€$ siphon**: check displayed credits against V's money before/after.
10. **Vanilla**: normal Ping, weapon hacking, existing icons, cyberdeck stats
   and other cyberdecks must continue working.
11. **XP**: normal Netrunner XP gain is expected and allowed; do not treat it as a bug.

**Keep PR #36 draft. REDscript compilation and gameplay results are
required before describing this feature as working.**

## Unchanged IDs
- `Items.NESTCyberdeck`
- `NetDeckInfiltration`
- `DeviceAction.NetDeckInfiltration`
- `QuickHack.NetDeckPuppetInfiltration`
- `DeviceAction.NetDeckVehicleInfiltration` (new native vehicle record)
