# UndiscoverMe Teleporter

Optional addon for **UndiscoverMe**. Install the UndiscoverMe core first.

Enable **Reveal additional map markers** in core settings (or the core catalog CET binding) to display the classified NCPD/Cyberpsycho/Tarot pins.

Open the world map and select an **UndiscoverMe-created pin**. Its tooltip now shows the game's native **Fast Travel** action, including the actual mouse or controller button and its hold-progress indicator. **Hold the mouse button shown on the tooltip** (or the mapped gamepad action) until the circle completes; V teleports to the pin and the map closes. A separate **F** key is not required.

The prompt uses Cyberpunk 2077's **own localized Fast Travel UI**. It automatically follows the game's interface language and current key/controller binding; no Teleporter translation overlay or separate Ink assets are required. The existing Polish and Korean `translation.json` packs continue to localize **Core's CET text**, not the game-owned Fast Travel prompt.

- Only pins actively registered by UndiscoverMe are eligible. Vanilla pins and other mods' pins remain unchanged, even if they share the same icon or coordinates.
- Regular Fast Travel still works on vanilla fast travel points. The addon never enables its tooltip on third-party or unrelated vanilla pins.
- Uses the map's **native** mouse/controller fast-travel hold route: `HandlePressInput` starts the hold, the game handles progress and early release, and `TryFastTravel` completes the teleport for UndiscoverMe-owned pins only.
- Works with the core's NCPD, Cyberpsycho and Tarot catalog proxies, plus its registered fast-travel proxies.
- Direct teleportation does not unlock locations, track quests or change discovery/completion state.
- Landing position is offset 1.5 meters above the pin. **There is no terrain/ground check yet.** Make a manual save before testing.
- **Verified in Cyberpunk 2077 2.5:** The native Fast Travel tooltip displays, holding the mouse button teleports V, and the fast-travel transition cleanup restores normal gameplay without leaving the screen blurred or player controls locked. The fix follows the game's loading-screen, hub-close, `OnBack`, and first-person-camera cleanup events.

## Install

Extract the addon's ZIP into the game folder after installing the latest UndiscoverMe core. The ZIP installs `r6/scripts/UndiscoverMeTeleporter/UndiscoverMeTeleporter.reds`. Remove this file to uninstall the addon. No duplicate copy of the core is included.

Requires UndiscoverMe core, redscript, and core dependencies. No separate CET hotkey or settings panel.

## Credits

Native tooltip implementation informed by the supplied **FastTravelFromAnywhereToAnyMapPin.reds** and **AccidentalFastTravelBugFix.reds** examples by **yakuzadeso**. UndiscoverMe only enables the action on its own registered pins.
