# Reveal Map Pins — exact content scope (Cyberpunk 2077 2.5)

This is a **marker-only exploration aid**. Do not replace vanilla game
discovery, fast travel progression, item collection, quest advancement, or
journal state. All runtime proxies must be independently removable.

## Display-only allowlist

| Category | Eligibility | Source |
| --- | --- | --- |
| Fast travel | Known and unknown vanilla fast-travel terminal positions; no unlocking | Native fast-travel-point records / placed terminals |
| NCPD Scanner Hustles | Parent category only; don't duplicate its subtypes | Journal-classified vanilla POIs |
| Assault in Progress | NCPD subset | Journal-classified minor activities |
| Suspected Organized Crime Activity | NCPD subset | Journal-classified minor activities |
| Reported Crime | NCPD subset; one primary marker, no subsequent cache-objective spam | Journal-classified minor activities |
| Cyberpsycho Sightings | Vanilla activity locations only; no quest objectives | Journal-classified POIs |
| Tarot cards | Existing tarot graffiti | Journal-classified POIs |
| Legendary and Iconic items | World-placed collectables where the exact item + position + availability is verified | Separate placed-loot and eligibility source (not in two cooked pin resources) |

NCPD Scanner Hustles **is a parent group, not a second marker**.

## Hard exclusions

- **All fixer locations** — the base game already displays them. Never add, hide, or alter original fixer pins.
- Main jobs, side jobs and all progress/objective/stage markers.
- Fixer gig types: Gun for Hire, Search and Recover, Thievery, Agent Saboteur,
  SOS: Merc Needed, Convoy, and any other gig/job.
- Service points: vendors, ripperdocs, weapon/clothing shops, drop points,
  Autofixer terminals, apartments, and other commercial locations.
- Arbitrary third-party map pins; original markers must never be altered.

## Verified data already supplied by user

WolvenKit 10.0.0-beta.1 exports from Cyberpunk game build 2500:

| Cooked file | Raw candidate rows | Distinct XYZ |
| --- | ---: | ---: |
| `03_night_city.poimappins.json` | 719 | 712 |
| `03_night_city.mappins.json` | 4465 | 3064 |

**These are not counts of eligible markers.** All rows have game X/Y/Z and a
`journalPathHash`; 45 exact positions occur in both resources. Many of the
4465 mappin rows are quest/stage/objective markers and must be rejected.
Never bulk-render these files or equate coordinate duplicates with duplicates
of the same journal entry.

### Journal extraction completed

The base and Phantom Liberty `cooked_journal.journal` JSON exports were
already provided. They supplied the journal-path classifications for the
212 retained candidates. The seven fixer service-point entries were dropped.
A **positive allowlist match** is required before a marker can be emitted.

Fast-travel positions and placed Legendary/Iconic items require separate
game-derived data. Merely finding an item TweakDB definition does not locate
that item in Night City. Random loot, vendor inventory and quest-exclusive
rewards are not static collectible markers.

## Current runtime safety gate

The registered-marker scanner **rejects all generic quest stages, gigs,
service points and unknown marker variants**. It accepts only registered
`PointOfInterestMappin` objects with explicit NCPD/cyberpsycho/tarot
variants, plus `FastTravelMappin`. This is intentionally conservative and
may omit valid locations until the journal-derived catalog is integrated.
It does not currently reveal all of the above list and does not yet
support world-placed Legendary/Iconic item locations.

Current category toggles: master, catalog, fast travel, NCPD,
cyberpsychos, tarot. Item toggles should be added **only after**
corresponding item marker data is implemented.

## Automated acceptance

1. Resolve journal paths and verify category classification.
2. Reject **100%** of excluded quest, gig, stage, vendor, service, and
   unclassified entries — fail closed.
3. Deduplicate by stable journal/entity identity and world position.
4. Do not render a proxy for an already visible vanilla marker; never
   mutate another mod's markers or unlock any destination.
5. Hide/clear our own markers when disabled or on session end.
6. Smoke-test a few representative markers in the world map. The player
   should not teleport to thousands of coordinates.

Game map fog/district discovery is a separate feature and is not implied
by revealing map markers.
