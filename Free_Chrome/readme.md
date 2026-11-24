# Free Chrome — All Cyberware Costs 0 Humanity

M1 Arc migration prototype, Cyberpunk 2077 2.50.

## Requirements
- RED4ext, redscript, TweakXL and M1 Arc 1.0.1 or newer.
- CET, Native Settings UI and Mod Settings are no longer required.

## Upgrade
Exit the game and remove the entire old `bin/x64/plugins/cyber_engine_tweaks/mods/FreeChrome/` directory (back up your db.json if desired). Install the new r6 and red4ext folders. Open Free Chrome in M1 Arc. The switch defaults ON; old CET preferences are not imported. Reload a save after each toggle. The mod changes only existing numeric standard Humanity cost flats (1–200, including Tinkerer variants); it preserves prices, capacity maximum, and slot restrictions. It applies at TweakXL startup, stores original values in namespaced process-local TweakDB backup flats, and restores only owned values when switched OFF. Save reloads no longer restore Humanity values. A full game restart removes the temporary backups. Diagnostics appear in M1 Arc Logs.

## Testing
Draft: verify the actual equipped and vendor implant costs on first load and after ON/OFF plus reload, repeated save transitions and full game restarts. No claim of in-game validation yet. Custom nonstandard cost paths formerly added to patch.lua are not automatically imported. If restoration fails, fully restart the game with the option disabled. For a setting changed at the main menu, fully restart before loading the first save if no active session is available to process the toggle.
