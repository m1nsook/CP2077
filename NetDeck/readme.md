# Infiltration — NetWatch Σ MK.1 / NETWORM

**Cyberpunk 2077 2.5 · v0.6.5 CRASH DIAGNOSTIC TEST · NOT game-compiler verified.**

**Mod:** Infiltration. **Deck:** NetWatch Σ MK.1. **Quickhack:** NETWORM.
**Source folder/prefix:** NetDeck. **Save-compatible item:** `Items.NESTCyberdeck`.

## v0.6.5 — diagnose NetDeck-induced streaming crashes (UNVERIFIED)

The player's repeatable A/B test is now the main evidence: **with NetDeck the
game crashes during map/world loading, without NetDeck it loads normally**.
The 17:04:28 crash again reported heap corruption near
`vehiclePersistentDataPS` and a streamed NPC. Other native reports mention
NPC stealth mappins and resource streaming. The faulting engine allocator and
the crash reporter's "unknown culprit" message cannot establish exactly
which NetDeck operation corrupted memory.

**Source isolation change:** both global `NPCPuppet` and `VehicleObject`
`OnGameAttached`/`OnDetach` wrappers are now removed. Those hooks previously
ran while the map streamed and maintained large weak-ref arrays on V.
Candidates are registered only when their native scanner action is generated
or a NETWORM upload explicitly starts on them. **Nearby unscanned enemy and
vehicle propagation may regress** until we develop a safer discovery path.

**Crash diagnostic addon** (included in the test ZIP):

`bin/x64/plugins/cyber_engine_tweaks/mods/NetDeckDiagnostics/init.lua`

This tiny CET mod uses observer-only hooks on *NetDeck methods*, not global
entity lifecycle methods. It emits `[NetDeck/DIAG v0.6.5]` lines:
session_start, observer_ready/unavailable, heartbeat every ~15 seconds,
wave_start, wave_infect (max 16 logged per upload), wave_finish,
hud_start, and hud_finish. Failing to observe a method is logged and
must not block gameplay. This does not read world objects during
the heartbeat. **Advanced Crash Reporter 1.6 already includes
CET `scripting.log`, so no patch to the crash reporter is needed.**

Read `bin/x64/plugins/cyber_engine_tweaks/scripting.log`, or send the
next Advanced Crash Reporter `.txt` (it includes the log tail). If it crashes
**before any `wave_start`**, we know the crashing session did not have a
logged successful NETWORM upload, though the item/quickhack source can still
affect the game. If it crashes after wave_start, the last method logs can
help correlate the crash with an upload or HUD transition. **Logging alone
is not proof of memory ownership or a definitive crash culprit.**

The previous 16:59:06 report also confirms REDscript compilation succeeded.
It included a RedHotTools hook and repeated unloaded-resource warnings, but
those **do not override the user's A/B evidence against NetDeck**.

First test loading the *same save/sector* with the new sources and logger
installed, **without using NETWORM**. If crashes remain, test script/tweak
components separately from the local `NetDeck.archive` icon archive. Do
not remove dependencies or mix source versions during comparisons. Preserve
a backup save, leave PR #36 in DRAFT, and **do not release** this build.

## v0.6.4 — map-load crash isolation (UNVERIFIED; PR #36 only)

**Player-confirmed isolation:** Cyberpunk 2077 continued crashing during map/world
loading with both Anywhere to Anywhere 2 and UndiscoverMe Teleporter removed.
After removing NetDeck, the game loaded successfully. This strongly
implicates NetDeck or an interaction with another installed mod, but it
does **not** identify the specific corrupting function.

The second crash report described heap corruption near an Animals NPC's
`senseComponent.OnSenseInitialize -> CreateSenseMappin`. NetDeck previously
wrapped **all** NPC `OnGameAttached`/`OnDetach` events to maintain a loaded
NPC registry. That is an especially risky place to touch player state and
arrays during streaming; it runs before a NETWORM upload.

**Targeted source change:** removed only the global
`@wrapMethod(NPCPuppet)` attach/detach wrappers. A target can still be
registered when the player scans it, or directly when NETWORM uploads to
that NPC. The targeting-system query, Sonic Shock Tier 5, device network
infiltration, and experimental native mission upload bar are preserved.

**Tradeoff:** passive discovery of unscanned NPCs no longer uses the
attach-time registry. Chain-spreading to nearby enemies **must be
reverified**; it may be less reliable than the previous version. This is
a crash-isolation patch, **not** a proven crash fix. Vehicle streaming hooks
remain and should be checked next if loading still crashes.

**Don't merge or release this build.** Back up a save. First establish
that map/world loading is stable in the same area, then test attacking
multiple enemies and connected devices. Static tests do not compile
REDscript or verify memory safety.

## v0.6.3 — optional native upload-bar experiment (UNVERIFIED)

The user requested the game's mission-style transfer bar in place of spammy
NETWORM HUD feedback. An experimental `UI_HUDProgressBar` implementation
was committed on **18 Nov 2026 at 16:30:42 Europe/Warsaw**, then reverted
at 16:31:42 before the user could install and test it. This was premature.

**The reported crash occurred at 16:29:37**, **65 seconds BEFORE** the HUD
feature was committed. The installed NetDeck version was last modified
at 16:13. Therefore the crash is **not evidence that this new HUD caused it**.
The crash report still has no identified culprit.

The HUD code has been restored for testing. It opens a native progress bar,
shows localized `NETWORM: UPLOADING TO ... TARGETS` and devices affected,
updates the progress over the bounded wave, and closes at completion or
unequip. It checks whether another quest's progress bar is active before
claiming it and verifies ownership by header before updating/closing.
**Those protections are source-level only and need real in-game testing.**

**Important:** This does not yet replace the separate `EXTRACTED DATA`
notification in `NetDeckNotify`. A native red completion summary remains
a planned, unverified follow-up. It also does not prove that the in-game
progress widget exactly matches the user's red malware-upload screenshot.

Keep PR #36 in DRAFT until game REDscript compilation and a safe in-game
test of both NETWORM spread and the upload UI. Preserve a save backup.
The crash investigation remains independent of this feature.

## v0.6.3 — enemy targeting and sound-safety changes (UNVERIFIED)

**Observed in the player's game (v0.6.2):** NETWORM counted 11 targets
around only a few gang members/cars, did **not** spread Weapon Glitch to
nearby enemies, and caused a persistent sound requiring mod removal.

Changes committed to PR #36:
- NPC quickhack now derives from Canto Mk.6's native
  `QuickHack.BaseBlackWallHack` enemy-targeting record instead of
  `QuickHack.FreePingHack`. NETWORM's own action/label/icon are retained.
  Blackwall start/completion effects are cleared. **Requires Phantom Liberty
  and in-game validation that no native Blackwall effects leak through.**
- Removed the old **must have Ping action** NPC scanner gate. Quest, dead,
  companion, crowd and child NPCs remain excluded.
- Added a bounded loaded-NPC fallback for enemies missed by the targeting
  system; existing device and vehicle paths are unchanged.
- Spread attempts happen every **2 seconds**, with a hard maximum of
  **16 scheduled jumps** per upload.
- Removed the repeated `PingNetworkGrid_Event` effect and per-hop reward
  notifications to address the persistent-audio regression. The precise
  audio source has not yet been proven.
- Final TARGETS feedback now counts dispatched effects, not all visited
  relays. The count is not evidence of successful spread in-game.

**Source-only: NOT REDscript-compiler verified. Do not reinstall v0.6.2 or
publish this test. Leave PR #36 draft until compilation, real enemy
propagation and normal audio teardown are verified on a backed-up save.**

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

## v0.6.2: previous vehicle-path experiment (failed in-game)

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

## v0.6.1: previous timed-wave experiment (failed in-game)

- One upload initializes a wave attached to the player and schedules a new
  step **every 2.0 seconds** in v0.6.3 (no simultaneous instant-burst loop).
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
the intended visible changes are staggered native Weapon Glitch effects.
Network pulses and repeated notifications were removed in v0.6.3. A custom NPC-to-NPC
beam requires a separate verified VFX implementation.

## Source-only install

Requires Cyberpunk 2077 2.5 **with Phantom Liberty**, RED4ext, REDscript, TweakXL and Codeware.
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
3. **Sound and count**: no repeating network pulses/notifications; check
   the one final `TARGETS: <number>`, and ensure all audio stops normally.
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
