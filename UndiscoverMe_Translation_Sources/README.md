# UndiscoverMe — Translation Sources

These are editable UTF-8 JSON dictionaries for the **UndiscoverMe Core** and **UndiscoverMe Teleporter**.
Teleporter adds no standalone text: it reuses the game's native Fast Travel action and map UI.

## Translate

1. Copy `en.json` to a new language file (for example `de.json`).
2. Set `locale` to the intended language code, such as `de-DE`.
3. Translate only the string **values** inside `strings`. Keep the keys, schema and JSON structure unchanged.
4. Save as UTF-8. Keep text concise for the Native Settings UI.
5. To make a game-ready language pack, place that JSON file at:
   `bin/x64/plugins/cyber_engine_tweaks/mods/UndiscoverMe/translation.json`.
6. Install the current English UndiscoverMe Core first, then extract the language pack into your game root and restart the game.

## Compatibility

- Language files add **text only**. They must not replace Core `init.lua`, REDscript gameplay files, TweakXL filters or `db.json`.
- Install **one translation pack at a time**. Polish and Korean use the same destination filename, so the last installed wins.
- Remove `translation.json` to restore English. Missing or invalid translations fall back to English.
- New community languages can use the same JSON schema without changing game scripts.
- CET binding labels, Native Settings UI labels/descriptions and UndiscoverMe marker captions are localized.
- **Mod Settings** (the separate REDscript-based mod) currently has static English annotations; its labels are not changed by these JSON overlays.
- The game-owned Teleporter hold instruction follows Cyberpunk 2077's language and is not overridden.
- Korean characters may require CET's Korean font support. Check your CET font configuration if text shows as boxes.

## Keys

`option.*` — labels and descriptions in the settings menu.

`ui.options` — Native Settings UI subcategory.

`binding.*` — CET binding descriptions.

`status.*` and `log.catalog` — player-facing CET status text.

`pin.*` — map marker captions.
