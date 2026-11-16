# Free Chrome — All Cyberware Costs 0 Humanity

**Cyberpunk 2077 2.5 + Phantom Liberty**

Free Chrome makes the game's **cyberware Humanity/Capacity cost** equal to **0** for eligible implants, without editing implant prices, maximum Cyberware Capacity, item stats, slot restrictions, or ripperdoc inventory. It is built for full-chrome power fantasies, build experiments, and screenshots.

## Features

- Automatically scans vanilla, Phantom Liberty, and *standardly registered* third-party cyberware records, instead of shipping a hardcoded list of item IDs.
- Changes only the TweakDB `variants` flat for recognized cyberware to `Variants.Humanity0Cost`.
- **On/off switch** in **Native Settings UI** and **Mod Settings**, plus a fallback CET Bindings hotkey (`FreeChrome_Toggle`).
- Persistent setting in an unshipped `db.json`. A switch to **OFF** restores the variant arrays captured before this mod changed them. No saved file is overwritten by the release.
- Re-scans at a modest interval for cyberware added after initial startup.
- Developer-friendly patch logic in `patch.lua`: change recognized cyberware areas / item types / tags without editing settings or packaging scripts.

## Important distinction

The base game uses the internal word **Humanity** for an implant's **Cyberware Capacity cost** (for example, `Variants.Humanity8Cost`). This is **not** the separate Humanity meter and cyberpsychosis system from *Wannabe Edgerunner*, *Dark Future*, or other mods. Free Chrome does **not** cancel those independent mods' Humanity loss from killing, activation of cyberware, or other events.

Zero cost does **not** increase your maximum Cyberware Capacity; it reduces the cost charged against it. It does not unlock additional physical slots or override other implant requirements.

## Installation

1. Install **Cyber Engine Tweaks** and **redscript** compatible with game 2.5 (and their prerequisites).
2. Extract the future **Free Chrome** release ZIP directly into the Cyberpunk 2077 game directory, preserving the `bin/` and `r6/` paths.
3. Launch the game and open **Native Settings UI → Free Chrome → General** or **Mod Settings → Free Chrome → General**. Both UIs are optional; CET's **Bindings** page provides a fallback toggle.
4. For reliable validation, visit a ripperdoc and check at least one installed/available implant. If an already equipped implant retains a cached cost, unequip and re-equip it to refresh the loadout view.

**To uninstall:** disable the mod first to restore the captured variant values in the running session, then exit and remove `bin/x64/plugins/cyber_engine_tweaks/mods/FreeChrome/` and `r6/scripts/FreeChrome/FreeChromeSettings.reds`. TweakDB runtime changes do not persist across full game restarts.

## Compatibility and limits

- **Vanilla + Phantom Liberty:** intended support for cyberware using normal TweakDB variant costs.
- **Modded cyberware:** recognized by standard cyberware equipment areas, `Cyb_` item types, or `Cyberware` tags. Mods defining new record types or entirely custom cost systems may need a patch.
- **Arms cyberware:** some have separate holstered-form capacity modifiers; the game may apply a cost from those separately. This first version does not rewrite those stats or promise to solve every special-case form.
- **Competing patches:** a different mod may change the same `.variants` flat. For reliable results, let Free Chrome apply *after* other cost patches; toggling back off restores the values captured when it applied.
- **Performance:** record enumeration is periodic (not every frame), and already-captured records aren't rewritten on every poll.
- **No change:** eddie purchase prices, implant stats, maximum Capacity, vendor inventory, equipment slots, or independent cyberpsychosis systems.

## Troubleshooting — 2.5 / Cyberware-EX

Seeing both settings switches ON does **not** prove the gameplay patch succeeded. The first in-game screenshot had functioning **Mod Settings + Native Settings UI**, but an equipped Defenzikov/Kerenzikov still showed a **16 Humanity/Capacity cost**. This branch includes a scanner correction for `gamedataItemType.Cyberware`, safer TweakDB IDs, and clearer patch diagnostics.

Check `bin/x64/plugins/cyber_engine_tweaks/cyber_engine_tweaks.log` and/or `bin/x64/plugins/cyber_engine_tweaks/mods/FreeChrome/scripting.log` depending on CET's logging configuration for messages like:

```text
[FreeChrome] records scanned=..., cyberware=..., matching Humanity variants=..., newly zeroed=..., failed=...
```

A **zero** value for `cyberware` or `matching Humanity variants` means the mod could not find usable records. **0 newly zeroed** can also mean all eligible records were already zeroed on a prior pass. A nonzero `failed` indicates writes rejected or not verified.

Reload the save/reopen the cyberware screen after switching. This runtime patch cannot promise that Cyberware-EX or a different UI immediately refreshes cached equipped-item Capacity.

## Status

**Experimental source build — in-game test required** before a public 1.0.0 release. Static checks cannot verify that every game/modded cyberware record is found or that the ripperdoc UI immediately refreshes on toggle.

The code is original to this repository; no assets or code are copied from other Humanity-cost mods.
