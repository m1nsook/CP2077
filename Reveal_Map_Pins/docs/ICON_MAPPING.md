# Native map icons — verified in WolvenKit game build 2500

These are **the player's own extracted Cyberpunk 2077 2.5 assets**,
inspected 2026-11-14. No atlas binary files, extracted PNGs, or game-owned
graphics are copied into this repository or the installable mod. The game
renders the sprites from its own resources.

## World-map POI record

The player exported `Mappins.StaticPointOfInterestMappinDefinition.json`
and the associated `possibleVariants.json` from the Tweak Browser.
Record type: `gamedataMappinDefinition_Record`.

- `showOnMap = 1`
- `showOnMinimap = 1`
- `showInWorld = 0`
- `visibilityRange = 0`
- 33 supported `possibleVariants`, including **all six** classified
  categories via `Mappins.PointOfInterest_*Variant` entries

The `gamedataMappinVariant` enum does **not** use the `PointOfInterest_`
TweakDB prefix. For example, use `gamedataMappinVariant.GangWatchVariant`,
not `gamedataMappinVariant.PointOfInterest_GangWatchVariant`.
The TweakDB record is `Mappins.StaticPointOfInterestMappinDefinition`.

## Icon assignment

All these native texture-part names exist in the extracted
`base/gameplay/gui/common/icons/mappin_icons.inkatlas` (three sets:
720p, 1080p, 2160p; each has 204 PNG sprites).

| Approved category | Game enum | `MappinUIUtils.MappinToTexturePart` | Atlas sprite |
| --- | --- | --- | --- |
| Assault in Progress | `GangWatchVariant` | `map_bounty` | `map_bounty.png` |
| Organized Crime | `OutpostVariant` | `map_bounty` | `map_bounty.png` |
| Reported Crime | `HiddenStashVariant` | `map_bounty` | `map_bounty.png` |
| Cyberpsycho Sighting | `HuntForPsychoVariant` | `hunt_for_psycho` | `hunt_for_psycho.png` |
| Tarot | `TarotVariant` | `tarot_card` | `tarot_card.png` |
| Fixer (not used by this mod; vanilla only) | `FixerVariant` | `fixer` | `fixer.png` |
| Fast travel (not yet in catalog) | `FastTravelVariant` | `fast_travel` | `fast_travel.png` |

`lootIconic.png` also exists in this native atlas but **does not mean
Iconic/Legendary item positions can be derived from the pin resources**.
A dedicated item-placement catalog and validated world-map profile are
needed before enabling item markers.

The user also supplied `atlas_map_pins.inkatlas` and `mappins.zip`
from `base/gameplay/gui/widgets/mappins/`. Its frame/layout sprites
include `circle`, `circle_glow`, `mappincontainer`,
`mappincontainer_border`, `quest_icon`, and `mappin_glow`, rather than
the category icons listed above.

World map icon selection happens through the game variant/Ink widget
pipeline. The game will supply its sprites. This project intentionally
does not reference or replace any bundled atlas textures.

## In-game caveat

A marker registered through CET `MappinSystem.RegisterMappin` is a
runtime marker, not a native saved `PointOfInterestMappin` journal object.
The appropriate icon variant and `showOnMap` definition are necessary,
but game 2.5 still needs a visual smoke test to verify sprite rendering,
tooltips, and filter behavior. Do not claim exact vanilla POI parity until
that test passes.
