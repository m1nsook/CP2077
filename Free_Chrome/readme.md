# Free Chrome — All Cyberware Costs 0 Humanity

**Cyberpunk 2077 2.5 + Phantom Liberty**

Free Chrome makes the game's **cyberware Humanity/Capacity cost** equal to **0** for eligible implants, without editing implant prices, maximum Cyberware Capacity, item stats, slot restrictions, or ripperdoc inventory. It is built for full-chrome power fantasies, build experiments, and screenshots.

## Features

- Targets the shared TweakDB **Humanity/Capacity cost modifier values**, which the game uses to calculate implant cost.
- Generates cost flat paths for standard `Variants.Humanity1Cost` through `Variants.Humanity200Cost`, including Tinkerer variants and compatible standard-named modded costs.
- Uses only `TweakDB:GetFlat()` and `TweakDB:SetFlat()` on relevant numeric `.value` fields; it does **not** modify cyberware item variants or every item's individual stats.
- **On/off switch** in **Native Settings UI** and **Mod Settings**, plus a fallback CET Bindings hotkey (`FreeChrome_Toggle`). Both settings labels explicitly say **reload save**, and both descriptions explain that a save must be loaded after switching ON or OFF.
- Persistent preference in an unshipped `db.json`. Switching **OFF** attempts to restore the numeric values captured before Free Chrome zeroed them.
- Rechecks the shared cost fields periodically (not per frame) to accommodate other mods changing cost values after startup.
- Developer-friendly `patch.lua` with `MAX_HUMANITY_COST` and `patch:addCostFlat("Your.CustomCost_inline1.value")` for alternate custom cost records.

## Important distinction

The base game uses the internal word **Humanity** for an implant's **Cyberware Capacity cost** (for example, `Variants.Humanity8Cost`). This is **not** the separate Humanity meter and cyberpsychosis system from *Wannabe Edgerunner*, *Dark Future*, or other mods. Free Chrome does **not** cancel those independent mods' Humanity loss from killing, activation of cyberware, or other events.

Zero cost does **not** increase your maximum Cyberware Capacity; it reduces the cost charged against it. It does not unlock additional physical slots or override other implant requirements.

## Installation

1. Install **Cyber Engine Tweaks** and **redscript** compatible with game 2.5 (and their prerequisites).
2. Extract the future **Free Chrome** release ZIP directly into the Cyberpunk 2077 game directory, preserving the `bin/` and `r6/` paths.
3. Launch the game and open **Native Settings UI → Free Chrome → General** or **Mod Settings → Free Chrome → General**. Both UIs are optional; CET's **Bindings** page provides a fallback toggle.
4. **After turning Free Chrome ON or OFF, load/reload a save** to apply the change to the equipped cyberware's Capacity calculation. Then open the cyberware screen or visit a ripperdoc to check implant costs.

**To uninstall:** disable the mod first to restore the captured modifier values in the running session, then exit and remove `bin/x64/plugins/cyber_engine_tweaks/mods/FreeChrome/` and `r6/scripts/FreeChrome/FreeChromeSettings.reds`. TweakDB runtime changes do not persist across full game restarts.

## Compatibility and limits

- **Vanilla + Phantom Liberty:** covers the standard, shared Humanity-cost modifier naming family.
- **Modded cyberware:** works when the mod uses the shared `Variants.Humanity<number>Cost` records. Custom cost records require explicitly registering their numeric paths with `addCostFlat()`.
- **Other Humanity/psychosis mods:** not affected if they use a separate meter or rule system.
- **Competing cost changes:** other mods writing the same shared `.value` flats can conflict. For consistent results, Free Chrome should apply last; disabling restores the last nonzero values it captured, except values subsequently taken over by other mods.
- **No change:** eddie purchase prices, cyberware item stats, maximum Capacity, inventory/vendor availability, or implant slot limits.
- **Performance:** scans a finite family of numeric modifier paths on startup and periodically thereafter, not every frame.

## Troubleshooting — game 2.5 / Cyberware-EX

The first in-game screenshots showed both switches ON, but Defenzikov/Kerenzikov still displayed a **16** Humanity/Capacity cost. That initial patcher targeted **individual cyberware item variants**, which did not demonstrate a working cost change.

The old 2.0-era `##ZeroHumanityCost` source supplied by the user instead changed numeric `Variants.Humanity*Cost*_inline*.value` modifiers directly, in 131 entries. Free Chrome now independently generates the equivalent family, with **129 nonzero/other relevant legacy paths covered**; the remaining two legacy paths were already Humanity 0.

Look for log lines such as:

```text
[FreeChrome] Humanity modifier fields found=..., newly zeroed=..., write failures=... (scanned ... candidate flats)
```

If `found=0`, the candidate records aren't available to CET. If `write failures` is nonzero, some writes weren't confirmed. If `newly zeroed=0` but `found` is positive, those fields may already have been set to zero on an earlier scan.

**Important: reload a save after each toggle.** Changing the TweakDB values is not enough to recalculate the cost of already-equipped cyberware in the current game session. The user confirmed that loading a save applies ON and OFF correctly. **Live test update (Nov 16, 2026):** The revised patch now shows **0 Humanity/Capacity cost** on equipped **FEEN-X Tier 5++** in the user's cyberware screen. The user also confirmed that switching Free Chrome **OFF restores the original cost**. Broader implant coverage and third-party cyberware compatibility have not been exhaustively tested.

### CET 1.38.1 write regression (identified November 16, 2026)

In the first 2.5 gameplay test, CET reported **129 modifier fields found, 0 newly zeroed, and 129 failed writes**. A direct read of `Variants.Humanity16Cost_inline2.value` still returned `16`. That rules out missing cost records.

CET issue [#1081](https://github.com/maximegmd/CyberEngineTweaks/issues/1081) reports that **`TweakDB:SetFlat()` inside Lua `pcall()` fails in CET 1.38.1**, even when the same call succeeds outside it. The previous Free Chrome patch accidentally wrapped writes inside `pcall()` at **two levels**: the TweakDB helper and the settings bridge (`init.lua`). Both have been corrected.

**Install both updated Lua files**, `init.lua` and `patch.lua`, from the same Free Chrome test branch. Restart the game completely. The ON scan should now report successful writes rather than 129 failures; test `print(TweakDB:GetFlat("Variants.Humanity16Cost_inline2.value"))` in CET:

- With Free Chrome **ON**, expect `0`.
- With Free Chrome **OFF**, expect the previous value (for this record, `16`).
- If a direct write now throws an error, CET will surface it instead of silently marking all 129 operations as failed. Capture that error for diagnosis.

**In-game confirmation:** With the corrected Lua files installed, FEEN-X Tier 5++ visibly shows **0** Humanity/Capacity cost. The user subsequently verified that the original cost returns when Free Chrome is disabled.

## Status

**In-game verified for ON/OFF in Cyberpunk 2077 2.5:** After loading a save, FEEN-X Tier 5++ displays 0 Humanity/Capacity cost with Free Chrome enabled, and its original cost returns when disabled and the save is loaded again. The toggle is **not instant** for already-equipped implant capacity calculations. Broader implant coverage and third-party cyberware still need testing.

The code is original to this repository; no assets or code are copied from other Humanity-cost mods.
