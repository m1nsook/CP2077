# UndiscoverMe

**Cyberpunk 2077 2.5 — experimental map-filter build**

## UndiscoverMe branding and native map UI (v0.4 experimental)

Player-visible name: **UndiscoverMe**. The internal CET mod folder
`mods/RevealMapPins`, REDscript module `MisoRevealMapPins`,
`miso_rmp_*` quest-fact bridge, existing hotkey IDs and `db.json`
**intentionally remain unchanged**. Do not rename or delete that folder:
keeping these stable prevents the existing working save settings and CET
bindings from being lost. All visible labels in CET, NSUI and Mod Settings
are branded UndiscoverMe.

An **optional** native World Map → **Custom** section is installed via
TweakXL at `r6/tweaks/UndiscoverMe/filters.yaml`:

- UNDISCOVERME  + (one extra row in the stock Custom filter list)
  - ALL SUPPORTED PINS
  - NCPD SCANNER
  - CYBERPSYCHO SIGHTINGS
  - TAROT
  - FAST TRAVEL (existing registered locations only; no complete terminal catalog yet)

The section is **collapsed by default** so the default 13-row filter list
doesn't grow longer. Selecting UNDISCOVERME hides the original filter rows
*inside that menu only* and shows these five shortcuts. Selecting it again
restores the original filter menu. This does not hide the original map pins.

The six rows are backed by *empty* map-filter records and intercepted by
REDscript, so they change only UndiscoverMe's existing fact/settings switches
instead of registering new marker variants or taking over a stock filter bit.
When enabling a mod category, its corresponding **vanilla filter is also
enabled in the map UI** as necessary for the original icon variants to render.
Turning off a mod category only removes our extra markers; it **never hides or
unregisters vanilla markers** or markers owned by other mods.

TweakXL **1.12.2+** (compatible with game patch 2.5) is required only for
the new native Custom filter rows; UndiscoverMe's proven 212-marker catalog,
quest completion read checks and CET hotkeys still work if the map extension
is absent. In-game map hook behavior is currently experimental and needs
a 2.5 smoke test; keep the prior PR #18 ZIP for immediate rollback.

This section is additional map UI, **not** an implementation of the
undiscovered fast-travel database, special loot markers, or fog reveal.



Requires RED4ext, redscript and Cyber Engine Tweaks. Mod Settings and
Native Settings UI are optional. Settings are persisted through CET
`db.json` and an internal quest-fact bridge.

## Current release scope — no fixer markers

This mod intentionally **does not manage fixer locations**. Cyberpunk 2077
already displays them by default. No additional fixer marker, hide/show logic,
category switch, or fixer catalog entry is included. The original game fixer
pins are never modified. The experimental game-derived catalog contains
**212** candidates: 114 Assault in Progress, 18 Organized Crime, 26 Reported
Crime, 17 Cyberpsycho Sightings, and 37 Tarot cards.

The prior 219-candidate catalog and PR #13–#16 fixer experiments are retained
in GitHub history for diagnostic context only. They are not current behavior.
Completion filtering for activities is still not fully verified on real saves.

## Confirmed scope (November 2026)

**Only display** missing *vanilla* fast-travel locations,
NCPD Scanner Hustles (Assault in Progress, Organized Crime, Reported Crime),
Cyberpsycho Sightings, Tarot cards, and eventually verified Legendary/Iconic
item locations. **Never display** story/side jobs, fixer gigs (Gun for Hire,
Search and Recover, Thievery, Agent Saboteur, SOS: Merc Needed, Convoy),
quest stages/objectives, **all fixers**, or service points (vendors, ripperdocs, drop points,
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

The category controls are: master, experimental catalog, fast travel, NCPD,
cyberpsycho, and tarot. No fixer setting exists; the game handles fixers. Existing `db.json` keys for retained switches are read normally;
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

## Experimental game-derived catalog (212 vanilla locations)

The new **classified catalog** is intentionally **off by default**. In CET
**Bindings**, assign **Reveal Map Pins: toggle classified catalog** and press
it once after loading a save. Alternatively use the optional Native Settings
UI switch. Turn it off to remove only the markers registered by this mod.

Catalog categories: **114 Assault in Progress, 18 Organized Crime, 26 Reported
Crime, 17 Cyberpsycho Sightings, and 37 Tarot** locations. The mod no
longer includes fixer locations or any fixer setting. Native fixer pins
are left untouched. No quest stages, gigs, shops or service locations
are registered. Fast travel and Legendary/Iconic item catalogs remain
separate unfinished milestones.

Runtime source: the player's WolvenKit game-2500 `03_night_city.poimappins`
and base+EP1 `cooked_journal.journal` exports. `catalog.tsv` contains only
212 selected journal hashes, native variant categories, and game coordinates.
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

## CET 1.38.1 sandbox startup repair (Nov 14, 2026)

Both RevealMapPins and NoTrackingPlease were failing to load because their
`init.lua` files called `debug.getinfo(1,"S")` at startup. The CET
v1.38.1 Lua sandbox intentionally excludes the `debug` library, so the
call fails before any hotkeys are registered. This was reproduced in a Lua
sandbox with `debug=nil` and independently checked against CET source
`src/scripting/LuaSandbox.cpp`.

The two scripts now use relative files (`db.json`,
`catalog_runtime.lua`, `catalog.tsv`), and only CET-supported
`onInit`, `onUpdate` and `onShutdown` events. Session state resets
on loss of the in-game quest system. Existing user `db.json` is preserved.

Regression tests: the original scripts fail with the matching nil-`debug`
error; corrected scripts load and register four RevealMapPins bindings
and one NoTrackingPlease binding, and read all 219 catalog entries.
Not yet verified in a running 2.5 game.


## Completed/finished activity filtering — game build 2500

**Fix for completed Cyberpsycho Sightings reappearing as undiscovered**:
the engine may remove a saved POI marker record after an activity completes.
`GetPointOfInterestMappinSavedState(hash)` can then return `found=false`;
the previous code incorrectly treated that as unfinished. The map icon itself
and the 219-location catalog were already confirmed in-game (PR #13).

The catalog now reads **three independent native sources**:

1. The existing POI marker saved phase (`CompletedPhase`).
2. The underlying POI's game journal state (`JournalManager.GetEntry` +
   `GetEntryState`).
3. If that location corresponds to a quest, its exact journal quest entry
   (`GetEntryByString(path, "gameJournalQuest")`), so `Succeeded` or
   `Failed` hides it even if the engine deleted the original map pin.

The bundled `quest_links.tsv` links **59** verified location hashes to
their original quest entries: all **17 Cyberpsychos**, **26 Reported Crime**,
**14 Organized Crime**, and **2 Assault in Progress** quests. Other
NCPD/POI records still use saved pin status and direct POI journal state.
The links are derived from the user's WolvenKit base+EP1 journals, not
guessed quest completion facts.

When either quest or POI state is completed/failed, the mod unregisters
its own marker on its regular 12-second refresh. A journal result is allowed to **remove a marker only when completion
is positively confirmed**. Unknown critical journal states are counted as
`unverified` and retain the otherwise-eligible marker. This preserves
the previously confirmed PR #13 native marker behavior while the exact
quest lookup behavior is tested on real saves. The mod never writes game
quest facts, advances quests, changes original markers, or unlocks fast travel. Original native icons are preserved.

The diagnostic hotkey now prints additional catalog counters:

- `completed`: locations hidden due to confirmed completed/failed state.
- `unverified`: locations hidden because reliable completion state could
  not be read (e.g. missing journal entry).
- `skipped`: all excluded locations, including completed/unverified,
  already-visible pins, missing DLC, and disabled categories.

If `unverified` is unexpectedly high on a save, send that diagnostic
line before reporting missing icons. This update has passed an offline
Lua game-system simulation (219 registered; completed and failed quests
removed; missing journal clears owned pins) but still needs live testing.


## PR #14 regression safety fix

The PR #14 completion check was **not safe on real save data**: a user
reported `catalog=219 active=0 skipped=219 completed=0 unverified=203 failed=0`
and the visible map pins disappeared. The new check interpreted
`unverified` as completed even though the game often returns no saved POI
record and some journal calls cannot resolve a quest.

This patch keeps PR #13's confirmed native marker/coordinate behavior:

- **Only a positively verified `Succeeded`, `Failed`, or native
  `CompletedPhase` hides a marker**. Unknown journal states leave that pin
  eligible and are counted separately.
- A missing `Game.GetJournalManager()` no longer clears the catalog.
  Native saved-state checks continue; unknown entries retain their markers.
- Within one game session, once an activity is positively recognized as
  completed, a transient loss of the journal manager will **not** bring
  its pin back. This cache resets when the save/session ends.
- CET diagnostics now include `questOK` (linked quests with a readable
  state) and `questUnknown` (linked quests without a readable state).
  These help distinguish incomplete quests from broken journal lookups.

This is a **safety repair**, not proof that every finished cyberpsycho can
be detected. The linked quest lookup must still be verified on a real
completed save. If the new diagnostics show `questOK=0` and a large
`questUnknown`, that remaining lookup problem requires further work
without making all map pins disappear again.

## Known regression & safe rendering reset (PR #16)

On a user's Cyberpunk 2077 2.5 save, PR #15 still showed completed
cyberpsycho markers and misleading fixer overlays. CET reported
`catalog=219 active=64 skipped=155 completed=0 unverified=194`.
The quest-status API was not actually verifying completed missions, and
the newer code is not suitable to ship as a completed-content filter.

**This build restores the exact game-tested PR #13 marker registration
and conservative saved-POI fallback**, with one new exclusion:
`points_of_interest/service_points/*_fixer` registry entries will
**never be registered as additional runtime proxies**. These seven
records are static service-point map positions, not reliable live
character coordinates; Cyberpunk itself owns the fixer pins and their
gig counters. No NCPD, tarot or cyberpsycho coordinates were changed.

This is **not** a fix for completed Cyberpsycho or NCPD quest display.
The working marker path is restored, but the original completed-quest
limitation remains until we can validate a true save-aware source.
The unused `quest_links.tsv` is included only for future development;
the restored runtime does not parse or require it. The CET diagnostics
show `catalog=219` and counts from the working runtime, not
`completed`, `unverified`, or `questOK`.

For reliable completion research, the game's save
`metadata.*.json` includes `finishedQuests` (quest IDs only).
Comparing the metadata from a player's completed save with the
journal classifications can establish known finished missions
without guessing fact names. Do not mutate player saves or upload
save data to a public repository.


## Save-aware completed activities (experimental, Cyberpunk 2077 2.5)

A provided `metadata.9.json` from a **game-2500 save** confirms **40 finished
activities among the 212 current map candidates** (23 Assault in Progress,
5 Organized Crime, 7 Reported Crime, and 5 Cyberpsychos). That
**private file is NOT bundled in this mod or committed to GitHub**.

The previous per-POI journal lookup attempts incorrectly hid 203 unknown
markers or reported zero finished activities. This version uses a different
read-only approach: the native `JournalManager.GetQuests` call with the
`succeeded` and `failed` filters, followed by each returned quest's
`GetId()` and `GetEntryState()`. It matches these real quest IDs against
`quest_ids.tsv`, an independently generated **175-row game-derived
catalog** (all NCPD + Cyberpsycho POIs; Tarot has no applicable quest ID).

**Safety behavior:** Only a game-journal quest returned in a terminal
`Succeeded` or `Failed` state can suppress an extra marker. Unknown
quests, missing JournalManager, malformed responses, or unavailable
`quest_ids.tsv` never cause the catalog to be cleared. Positively
completed entries are remembered for the current game session only;
no game facts or saves are modified. All native map icons, coordinates
and four CET bindings stay as in the last tested rendering path.

The CET diagnostics now print:

- `catalog`: 212 total candidates (not a promise of visible count)
- `completed`: catalog locations skipped because their quest is verified finished
- `journalQuests`: returned quest entries whose state could be read
- `journalMatched`: terminal quest IDs matching our map catalog
- `active`, `skipped`, `failed`: existing marker counters

The uploaded save metadata gives a useful **40-match benchmark**, but the
runtime number is not guaranteed to equal 40 until tested in-game; some
finished minor activities may be missing from the journal query. If the
CET log prints `Live quest completion unavailable`, the original known
working map behavior is preserved, not an empty map.

To validate the exact catalog without including private progress in source
control:

`python tools/validate_quest_ids.py --metadata path/to/metadata.9.json`

This reads only the local JSON and prints aggregate counts. Do not
share or commit other players' save metadata.
