# Action Blocked Fix

Experimental automatic recovery for Cyberpunk 2077 2.5. Requires redscript only. No M1 Arc, RED4ext plugin, CET, settings, buttons or manifest.

Once per game second, the mod checks whether NoCombat remains active during on-foot combat in scene tier 1. After eight consecutive qualifying samples it removes that single status effect once. It rearms only after ten clean noncombat samples or a player/session transition. It does not change quest facts, combat flags, phone state, equipment or any other status effect.

## Test

- Remove the previous prototype's directory `red4ext/plugins/m1arc/mods/m1nsook.action-blocked-fix` if installed.
- Install the game-root ZIP and keep a backup of your affected save.
- On a healthy save, test dialogue, safe zones, vehicles, and normal combat.
- On an affected save, enter genuine on-foot combat, wait at least eight gameplay seconds, and test weapons and cyberware.
- Uninstall by deleting `r6/scripts/ActionBlockedFix`.

## Limitations

Experimental heuristic, not a proven vanilla root-cause fix. Combat, scene tier and mounted state are checked, but independent phone, safe-zone and quest restriction ownership checks are not yet verified. Legitimate overlapping restrictions may still be affected. The prototype cannot repair locks that do not involve NoCombat or that prevent the player from entering combat. Do not release as a stable fix until healthy and affected saves have been tested and the ownership checks are resolved.
