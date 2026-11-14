# UndiscoverMe Teleporter

Optional addon for **UndiscoverMe**. Install the UndiscoverMe core first.

Open the world map, hover an **UndiscoverMe-created pin**, then **hold the game's Fast Travel action** (F on the default keyboard layout) until the native hold indicator completes. V teleports to the pin and the map closes.

- Only pins actively registered by UndiscoverMe are eligible. Vanilla pins and other mods' pins remain unchanged, even if they share the same icon or coordinates.
- Regular Fast Travel still works on vanilla fast travel points.
- Works with the core's NCPD, Cyberpsycho and Tarot catalog proxies, plus its registered fast-travel proxies.
- Direct teleportation does not unlock locations, track quests or change discovery/completion state.
- Landing position is offset 1.5 meters above the pin. **There is no terrain/ground check yet.** Make a manual save before testing.
- Experimental on Cyberpunk 2077 2.5. REDscript compilation and in-game behavior still require testing.

## Install

Extract the addon's ZIP into the game folder after installing the latest UndiscoverMe core. The ZIP installs `r6/scripts/UndiscoverMeTeleporter/UndiscoverMeTeleporter.reds`. Remove this file to uninstall the addon. No duplicate copy of the core is included.

Requires UndiscoverMe core, redscript, and core dependencies. No separate CET hotkey or settings panel.
