# UndiscoverMe — Translation Sources

These are editable UTF-8 JSON dictionaries for **UndiscoverMe Core**. The optional **UndiscoverMe Teleporter** displays the game's **native Fast Travel tooltip, hold icon and progress ring** on its registered markers. Those words and button prompts belong to Cyberpunk 2077 and automatically follow the **game's interface language** (and current input binding), not the Core translation JSON.

**No Teleporter-only keys need translation.** Do not add unused `teleporter.*` keys: they would not change the native tooltip.

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
- The Teleporter's visible Fast Travel label, button glyph and hold-progress UI use the game's own localization. To see Polish/Korean here, set the **game's interface language** to Polish/Korean; installing a Core `translation.json` overlay alone does not change the game's Fast Travel tooltip.
- Korean characters may require CET's Korean font support. Check your CET font configuration if text shows as boxes.

## Keys

`option.*` — labels and descriptions in the settings menu.

`ui.options` — Native Settings UI subcategory.

`binding.*` — CET binding descriptions.

`status.*` and `log.catalog` — player-facing CET status text.

`pin.*` — map marker captions.
