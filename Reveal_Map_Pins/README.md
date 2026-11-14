# Reveal Map Pins

**Cyberpunk 2077 2.5 — experimental build**

Requires RED4ext, redscript and Cyber Engine Tweaks. Mod Settings and
Native Settings UI are optional. Settings are persisted through CET
`db.json` and an internal quest-fact bridge.

## Confirmed scope (November 2026)

**Only display** missing *vanilla* fast-travel locations, fixers (not their gigs),
NCPD Scanner Hustles (Assault in Progress, Organized Crime, Reported Crime),
Cyberpsycho Sightings, Tarot cards, and eventually verified Legendary/Iconic
item locations. **Never display** story/side jobs, fixer gigs (Gun for Hire,
Search and Recover, Thievery, Agent Saboteur, SOS: Merc Needed, Convoy),
quest stages/objectives, or service points (vendors, ripperdocs, drop points,
Autofixer, apartments, etc.).

The runtime scanner now fails closed by **explicit marker variant + game
mappin class**, instead of cloning a generic WorldMap filter category.
QuestMappin objects and all unknown sources are excluded; this can miss some
otherwise eligible markers until the offline journal catalog is ready.
The installed game registry is still incomplete: none of this promises
full-map reveal yet. No third-party markers are touched or moved.

**Item markers are not implemented yet.** The game's cooked mappin/POI
tables alone do not identify every world-placed Legendary/Iconic loot item.
An independently verified loot-placement dataset will be needed. A misleading
toggle for unimplemented items has deliberately not been added.

The six active switches are: master, fast travel, fixer, NCPD, cyberpsycho,
and tarot. Existing `db.json` keys for retained switches are read normally;
obsolete job/service keys are ignored. Mod Settings and optional NSUI match
these switches.

## Vanilla marker compatibility

- Only explicitly allowlisted **game-managed** POI and fast-travel markers are considered
  for temporary map display proxies; quest-stage objects are never proxied.
- Generic runtime markers, stubs, and unknown marker classes are ignored,
  even when they use vanilla icon variants. That protects typical third-party
  garages, chop shops, custom waypoints and other runtime pins.
- Proxies now use game TweakDB records with **showOnMap = true**, rather than
  the generic static mappin definition.
- The current registry preserves the original variant instead of changing a
  category into a generic quest/exclamation icon.
- The master/category switches remove only proxies made by Reveal Map Pins.

**Still not supported:** a complete allowlisted coordinate catalog, actual
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
5. Verify only allowed vanilla markers and that third-party vehicle shop pins
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

## Experimental game-derived catalog (219 vanilla locations)

The new **classified catalog** is intentionally **off by default**. In CET
**Bindings**, assign **Reveal Map Pins: toggle classified catalog** and press
it once after loading a save. Alternatively use the optional Native Settings
UI switch. Turn it off to remove only the markers registered by this mod.

Catalog categories: **114 Assault in Progress, 18 Organized Crime, 26 Reported
Crime, 17 Cyberpsycho Sighting, 37 Tarot and 7 Fixer** locations. Categories
respect existing NCPD/Cyberpsycho/Tarot/Fixer switches. The mod never
registers quest stages, gigs, shops, or service locations. Fast travel and
Legendary/Iconic item catalogs remain separate unfinished milestones.

Runtime source: the player's WolvenKit game-2500 `03_night_city.poimappins`
and base+EP1 `cooked_journal.journal` exports. `catalog.tsv` contains only
219 selected journal hashes, native variant categories, and game coordinates.
**The 71 MB journals are not packaged.** This registry is build-specific.

The CET owner reads the game's saved POI phase (completed content is skipped),
compares its candidate positions/variants to visible map pins, and owns its
registered IDs. It resynchronizes every 12 seconds. If map enumeration or
saved-state calls fail, it skips marker registration rather than duplicating
unknown sources. While the catalog is enabled, the REDscript scanner limits itself to
registered hidden **fast-travel** markers, avoiding overlap with the 219
classified POIs. A complete unregistered fast-travel catalog still needs
separate extraction.

**Native map icons** are selected from the game variant, not shipped as new
assets. Relevant WolvenKit paths:

- `base/gameplay/gui/common/icons/mappin_icons.inkatlas`
- `base/gameplay/gui/widgets/mappins/atlas_map_pins.inkatlas`
- `base/gameplay/gui/widgets/mappins/quest/default_mappin.inkwidget`

The registration uses the vanilla map-capable
`Mappins.StaticPointOfInterestMappinDefinition` record.

**Not tested in-game yet:** the marker controller, icon styling, filtering and
save-state behavior must be smoke-tested on Cyberpunk 2077 2.5. Runtime pins
are not the same engine type as real journal-owned `PointOfInterestMappin`
objects, so tooltips and the special fixer profile may differ. The mode is
opt-in for that reason.

## CET Bindings startup repair (2026-11-14)

The initial 219-pin catalog PR #11 test ZIP could load no **Reveal Map
Pins** bindings in CET: `catalog_runtime.lua` created a `TweakDBID`
at Lua module load time, before CET's `onInit` event (when game RTTI
becomes accessible). If this raised an error, the main `init.lua`
stopped **before registering its four hotkeys**.

This update moves engine-record creation into the in-game registration
path, defers catalog loading until `onInit`, and protects it with
`pcall`. Even if the data or module is missing, all four hotkeys
remain registered and the CET console explains the catalog error.

**Verify:** After extracting the updated ZIP into the game folder,
restart the game (not only Reload All Mods) and open CET > Bindings.
Look for the `Reveal Map Pins` group with master, catalog, diagnostics,
and temporary-marker-test hotkeys. CET prints
`[RevealMapPins] CET initialized; four Reveal Map Pins bindings registered`
when the mod entrypoint executes. If this line is missing, verify
`bin/x64/plugins/cyber_engine_tweaks/mods/RevealMapPins/init.lua`
exists and inspect CET's log for a Lua load failure. The test ZIP
does **not** include `db.json`; it preserves your choices.

This is a local Lua startup regression-tested fix, **not** yet an
in-game confirmation that all 219 markers render correctly.
