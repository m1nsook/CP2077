# Action Blocked Fix

Experimental Cyberpunk 2077 2.5 recovery prototype for stuck weapon/cyberware action restrictions. Requires redscript, RED4ext and M1 Arc 1.0.1+ (not CET).

The mod observes NoCombat, combat, scene tier and mounted state once per game second and logs state changes. Three consecutive samples of NoCombat during full-gameplay on-foot combat are flagged as a candidate only. It never automatically removes anything. It does not reset quests, combat, phone, equipment or player state machines.

## Test
- Install game-root files from the development artifact; keep a backup of the affected save.
- Start a healthy save, visit a safe zone, start/end dialogue and enter/exit a vehicle. Inspect state and confirm no gameplay changes.
- Load the affected save and inspect. If NoCombat is absent, share the logs instead of attempting a broad reset.
- If NoCombat is present and V is in genuine combat on foot outside intentional restrictions, wait for ABF_CANDIDATE. In M1 Arc choose Repair stuck NoCombat, close the overlay and test drawing a weapon and using cyberware.
- Copy Diagnostics before/after. ABF_EFFECT_CLEARED confirms only the record is gone, not that input was restored. Do not save if quest behavior is abnormal.
- Uninstall by deleting the two mod directories under r6/scripts and red4ext/plugins/m1arc/mods.

## Limitations
Scene/vehicle/combat are necessary but not sufficient evidence of an orphaned restriction. Safe-zone, phone and specific quest ownership checks still need exact 2.5 vanilla scripts and a reproducible affected save. Repair is intentionally manual and rejects unknown contexts, absent NoCombat, noncombat and non-full-gameplay samples. No generic automatic fix yet. This development build must not be released as a verified fix.

Reference for the reported symptom: https://steamcommunity.com/app/1091500/discussions/7/3108016147997747607/
Reference for NoCombat as a candidate: https://www.nexusmods.com/cyberpunk2077/mods/34427?tab=posts (mod-related report, not evidence of the vanilla root cause).
