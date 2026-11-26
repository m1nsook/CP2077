# Serce — Main Menu Music

Replaces the main menu music with Serce (Underground / CMAZ), from 102.0 HKGN FM.

## Requirements

- Cyberpunk 2077 2.50
- [Immersive Main Menu Audio](https://www.nexusmods.com/cyberpunk2077/mods/22695)
- [Audioware](https://www.nexusmods.com/cyberpunk2077/mods/12001) and its dependencies
- REDscript
- Mod Settings (optional)

## Installation (development)

The song is not distributed with this repository. Obtain the track from your own game installation and export it as `serce.wav`. The vanilla audio event is `mus_radio_15_serce`; its Wwise media ID is not yet verified. Do not confuse the event ID with the media ID.

Place `serce.wav` next to `audios.yaml` under `r6/audioware/MainMenuMusic/`. Copy the resulting `r6` folder into the game directory. Remove any other manifest registering `MainMenuMusic` before installation (back it up first). Do not install multiple main menu music packs at once.

## Uninstallation

Remove this pack's `audios.yaml` and `serce.wav` from `r6/audioware/MainMenuMusic/`, then restart the game. Restore the previous music pack if desired.

## Status

Development only. Audio asset identification and in-game testing are pending.
