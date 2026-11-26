# Serce — Main Menu Music

Replaces the main menu music with Serce (Underground / CMAZ) from 102.0 HKGN FM.

[Nexus Mods](https://www.nexusmods.com/cyberpunk2077/mods/39716)

## Requirements

- Cyberpunk 2077 2.50
- [Immersive Main Menu Audio 1.4](https://www.nexusmods.com/cyberpunk2077/mods/22695)
- [Audioware](https://www.nexusmods.com/cyberpunk2077/mods/12001) and its dependencies
- REDscript

## Installation from GitHub

The GitHub package contains the configuration, not the original game audio.

1. In WolvenKit, locate and export `807764327.wem` from your own game as WAV.
2. Rename the exported file to `serce.wav` and place it beside `audios.yaml` in `r6/audioware/MainMenuMusic/`.
3. Copy the `r6` folder into your game directory. Back up and remove any other manifest registering `MainMenuMusic` first.

The vanilla event is `mus_radio_15_serce` (event ID `3385271940`); the media ID is `807764327`.

## Uninstallation

Remove this pack's `audios.yaml` and `serce.wav` and restart the game. Restore your previous music pack if needed.

## Status

v1.0.0 — in-game playback confirmed by the author on Cyberpunk 2077 2.50.
