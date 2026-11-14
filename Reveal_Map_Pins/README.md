# Reveal Map Pins

Cyberpunk 2077 2.5 map marker reveal mod. Experimental; test before using a long-term save.

## Compatibility: vanilla only

Only hidden game-managed `PointOfInterestMappin`, `QuestMappin`, and `FastTravelMappin` objects
are candidates for temporary display proxies. **Generic `RuntimeMappin` and `StubMappin`
are excluded**, including markers from other mods that reuse vanilla icons or filter groups.

This is designed to prevent duplicate vehicle-garage / chop-shop markers from
Steal Claim or Sell Any Vehicles and other mods that call `RegisterMappin`.
Some vanilla runtime markers will not be revealed by this stricter rule.

**Limitation:** the game does not expose a reliable `createdByMod` field.
A third-party mod that adds journal-backed `PointOfInterestMappin` or `QuestMappin`
may still look game-managed. No general source-provenance guarantee is possible
without a trusted catalog of original marker IDs.

Reveal Map Pins never changes the third-party marker itself. When a source
becomes visible, disappears, or is no longer eligible, only a temporary proxy
created by Reveal Map Pins is removed. It does not unlock fast travel,
start or complete missions, or reveal geographical map fog.

## Smoke test

1. Back up your save and install this update by merging the mod folder's
   `bin/` and `r6/` contents into the Cyberpunk 2077 game folder.
2. Load a save with the third-party vehicle garage mod installed.
3. Check each garage/chop-shop icon: only its original marker should appear.
4. Check hidden vanilla gigs, NCPD markers and fast travel pins.
5. Turn off the Reveal Map Pins master toggle. Only its proxies should disappear.
6. Restore the toggle, save, reload, and check the map again.

Settings continue to persist in the CET `db.json`, and both Mod Settings
and Native Settings UI remain optional.
