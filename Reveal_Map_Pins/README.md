# Reveal Map Pins

**Cyberpunk 2077 2.5 — experimental build**

Requires RED4ext, redscript and Cyber Engine Tweaks. Mod Settings and
Native Settings UI are optional. Settings are persisted through CET
`db.json` and an internal quest-fact bridge.

## Vanilla marker compatibility

- Hidden **game-managed** POI, quest, and fast-travel markers are considered
  for temporary map display proxies.
- Generic runtime markers, stubs, and unknown marker classes are ignored,
  even when they use vanilla icon variants. That protects typical third-party
  garages, chop shops, custom waypoints and other runtime pins.
- Proxies now use game TweakDB records with **showOnMap = true**, rather than
  the generic static mappin definition.
- Some unsupported vanilla icon variants appear as a generic POI exclamation
  mark. This avoids injecting new TweakDB dependencies.
- The master/category switches remove only proxies made by Reveal Map Pins.

**Still not supported:** revealing unregistered future quest content, actual
map fog/undiscovered districts, or unlocking fast-travel destinations.
Third-party mods that inject journal-backed POI/quest markers cannot always be
distinguished from vanilla sources; complete provenance detection is not
possible using the public `IMappin` API.

## Native Settings UI

When Native Settings UI is installed, find **Reveal Map Pins** in its Mods
settings page. The CET adapter now retries registration if NSUI loads later.
It also logs registration errors to the CET log.

When NSUI is missing, the CET Bindings master hotkey and Mod Settings remain
available independently.

## Verify

1. Back up your save, then update the mod files in `bin/` and `r6/`.
2. Load a save, open the world map and choose **All** under game map filters.
3. In CET **Bindings**, assign **Reveal Map Pins: print map scan diagnostics**
   and press it after the game has loaded for several seconds.
4. The CET console prints `seen`, `hidden`, `eligible`, `active`,
   and `failed` counters. If eligible is zero, the scanner found no
   eligible hidden vanilla markers. If eligible is positive but active is
   zero, registration failed. If active is positive yet nothing is shown,
   investigate visibility filtering / map widget compatibility.
5. Verify vanilla gigs/NCPD markers and that third-party vehicle shop pins
   do not duplicate.
6. Toggle the mod off; other mods' original markers must remain.

**This is a static code fix, not yet verified with a running 2.5 game.**
If the game reports a compile error, share `r6/logs/redscript_rCURRENT.log`.

## Startup check

On session load, the CET companion now explicitly requests the REDscript
`RMPSystem` scanner and `RMPSettings` bridge. A missing system emits one
warning in the CET console instead of silently doing nothing. The CET
**print map scan diagnostics** binding reports `scanner=initialized` or
`scanner=MISSING` alongside the existing map scan counters.

This startup fix doesn't create a complete catalog of undiscovered vanilla
locations; it first makes the existing registered-marker scanner observable.


## Temporary map-rendering test (optional)

This test distinguishes a broken rendering path from a missing location
catalog. It uses CET directly and **does not require the REDscript scanner**.

1. In CET **Bindings**, assign `Reveal Map Pins: toggle temporary marker test`.
2. Load a save, press the hotkey **once**, then open the world map near V's
   apartment (Megabuilding H10, Watson).
3. Check for a *temporary quest/exclamation-style marker* at
   `(-1382.2, 1270.6, 123.08)`. This is a verified vanilla coordinate.
   It may overlap an existing apartment icon, so zoom in if needed.
4. Press the same hotkey again to remove the test marker. The temporary marker
   is also removed on session exit or shutdown.

The CET console logs whether the marker registration returned an ID or
failed. If this probe appears but the regular mod reveals nothing, the
problem is the vanilla marker source/filter/catalog path, **not** CET's ability
to register a marker. If it doesn't appear, we must investigate map UI profiles
or registration before building a large catalog.

This test adds **no** gameplay progression, fast-travel unlocks, permanent
markers, or third-party dependencies. It is not a replacement for the actual
vanilla location catalog.

## Implementation reference (research only)

Disclosed Hidden Gems v1.1.1 demonstrates a separate catalog of 313 authored
world coordinates, a `ScriptableSystem` that registers its own IDs at
`Session/Ready`, per-marker visibility and completion state, a dedicated
world-map UI profile, and an additional map filter. It uses a custom marker
variant and UI/tweak resources. Those techniques inform our design; we **do
not redistribute its curated coordinates, code, UI, or assets**.

For Reveal Map Pins we want a **game-derived** vanilla catalog with automatic
validation and collision/deduplication against pre-existing map markers.
No player should manually visit hundreds of locations.

## REDscript enum compatibility (2026-11-14)

The earlier PR #8 test build **did not compile** on Cyberpunk 2077 2.5: `gamedataMappinVariant.PointOfInterest_ExclamationMarkVariant` is a TweakDB-style name, **not** a `gamedataMappinVariant` member. The REDscript map-proxy code also had six other mismatched member names, and the CET test used the same invalid exclamation-mark value. These names have been corrected against the generated RED4ext `game::data::MappinVariant` enum, including `Zzz01_`, `Zzz02_`, `Zzz05_` and `Zzz06_` prefixes. No game-side compile has yet been run; recheck `r6/logs/redscript_rCURRENT.log` after installation.
