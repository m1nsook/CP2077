<a href="https://www.nexusmods.com/profile/m1nsook"><img width="51" height="51" alt="Nexus Mods profile" src="https://github.com/user-attachments/assets/8fd6966d-74fc-417d-b09f-28bf5a8e0ac9" /></a>

# CP2077 Mods

[![License](https://img.shields.io/github/license/m1nsook/CP2077)](LICENSE)
[![GitHub Release](https://img.shields.io/github/v/release/m1nsook/CP2077)](https://github.com/m1nsook/CP2077/releases)

Source code and releases for my Cyberpunk 2077 mods.

## Mods

| Mod | Description | Status |
| --- | --- | --- |
| [No Tracking Please](No_Tracking_Please) | Blocks enemy netrunner/police position reveals and quickhack trace acceleration. | Active |
| [UndiscoverMe](UndiscoverMe) | Reveals eligible NCPD Scanner, Cyberpsycho and Tarot map markers without reopening completed activities. | Active |
| [UndiscoverMe Teleporter](UndiscoverMe_Teleporter) | Optional addon: hold the map's Fast Travel action on an UndiscoverMe pin to teleport. Requires UndiscoverMe core. | In-game tested |

UndiscoverMe does **not** add fixer locations, gigs, shops, or quest objectives. Fast-travel marker support is limited; a complete fast-travel and Legendary/Iconic item catalog is not available yet.

## Translations (optional)

English is built into **UndiscoverMe Core** and **UndiscoverMe Teleporter**. No English language pack is needed.

| Downloadable package | Contents |
| --- | --- |
| [Polski](UndiscoverMe_Polish) | Polish CET / Native Settings UI text and map-pin captions. |
| [한국어](UndiscoverMe_Korean) | Korean CET / Native Settings UI text and map-pin captions. |
| [Translation Sources](UndiscoverMe_Translation_Sources) | Editable English, Polish and Korean dictionaries for translators; **not an installable mod**. |

Install Core first (Teleporter optional), then extract **one** language ZIP into the Cyberpunk 2077 directory and restart. Each language overlay installs only `bin/x64/plugins/cyber_engine_tweaks/mods/UndiscoverMe/translation.json`; install only one at a time. Delete that file to restore English. Existing `db.json` and all gameplay files are preserved.

The separate REDscript **Mod Settings** interface has static English annotations and is not translated by this CET/NSUI overlay. Cyberpunk's own map action prompts remain controlled by the game's language. Korean may require CET font support.
## Install

Download the mod ZIP from [Releases](https://github.com/m1nsook/CP2077/releases) and extract its `bin/` and `r6/` folders into your Cyberpunk 2077 game directory. Install each mod separately. Source folders and tools are not part of the game installation.

## Dependencies

| Dependency | No Tracking Please | UndiscoverMe | Teleporter addon |
| --- | --- | --- | --- |
| UndiscoverMe core | — | — | Required |
| RED4ext | Required | Required | Via core |
| redscript | Required | Required | Required |
| Cyber Engine Tweaks (CET) | Required | Required | Via core |
| TweakXL | Not required | Required for extra map filter | Not required |
| Mod Settings | Optional | Optional | Not required |
| Native Settings UI | Optional | Optional | Not required |
| ArchiveXL / Codeware | Not required | Not required | Not required |

UndiscoverMe's basic marker functionality works without TweakXL; its separate **Filter: UndiscoverMe** entry requires it. The new map-filter UI is not yet confirmed working in-game.

The Teleporter addon is a separate download, never bundled into the core ZIP. It uses the native hold action and does not alter map filter pills. Test on a manual save: ground safety checks are not yet implemented.

## Publish one mod (maintainers)

Open **Actions → Release a mod → Run workflow** (branch `main`), choose a mod, language pack, or the translation-source bundle, and enter a version such as `1.0.0`, `v1.0.0`, or `1.0.0-dev.1`. The `v` prefix is optional; release tags always include it.

Each manual run publishes **only the selected mod**, including an installable ZIP, SHA-256 checksum and short release notes. Tags are mod-specific (for example `undiscoverme-v0.5.0` and `undiscoverme-teleporter-v0.1.0`), so versions are independent. Existing releases are never overwritten. **Normal pushes, PRs and tag creation do not run this workflow.**

Issues and feature requests: [GitHub Issues](https://github.com/m1nsook/CP2077/issues).
