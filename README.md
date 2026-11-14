<a href="https://www.nexusmods.com/profile/m1nsook"><img width="51" height="51" alt="Nexus Mods profile" src="https://github.com/user-attachments/assets/8fd6966d-74fc-417d-b09f-28bf5a8e0ac9" /></a>

# CP2077 Mods

[![License](https://img.shields.io/github/license/m1nsook/CP2077)](LICENSE)
[![GitHub Release](https://img.shields.io/github/v/release/m1nsook/CP2077)](https://github.com/m1nsook/CP2077/releases)

Source code and releases for my Cyberpunk 2077 mods.

## Mods

| Mod | Description | Status |
| --- | --- | --- |
| [No Tracking Please](No_Tracking_Please) | Blocks enemy netrunner/police position reveals and quickhack trace acceleration. | Active |
| [UndiscoverMe](UndiscoverMe) | Reveals eligible NCPD Scanner, Cyberpsycho and Tarot map markers without reopening completed activities. Includes an experimental fifth world-map filter. | Active / map filter experimental |

UndiscoverMe does **not** add fixer locations, gigs, shops, or quest objectives. Fast-travel marker support is limited; a complete fast-travel and Legendary/Iconic item catalog is not available yet.

## Install

Download the mod ZIP from [Releases](https://github.com/m1nsook/CP2077/releases) and extract its `bin/` and `r6/` folders into your Cyberpunk 2077 game directory. Install each mod separately. Source folders and tools are not part of the game installation.

## Dependencies

| Dependency | No Tracking Please | UndiscoverMe |
| --- | --- | --- |
| RED4ext | Required | Required |
| redscript | Required | Required |
| Cyber Engine Tweaks (CET) | Required | Required |
| TweakXL | Not required | Required for fifth map filter |
| Mod Settings | Optional | Optional |
| Native Settings UI | Optional | Optional |
| ArchiveXL / Codeware | Not required | Not required |

UndiscoverMe's basic marker functionality works without TweakXL; its separate **Filter: UndiscoverMe** entry requires it. The new map-filter UI is not yet confirmed working in-game.

Issues and feature requests: [GitHub Issues](https://github.com/m1nsook/CP2077/issues).
