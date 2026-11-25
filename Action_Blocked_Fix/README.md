# Action Blocked Fix

Experimental automatic recovery for Cyberpunk 2077 2.5. Requires REDscript only.

## Confirmed reproduction

An affected save showed SceneTier=1, HighLevel=1, Zones=1, UpperBody=5, Vitals=0, no combat, no vehicle, ForceEmptyHands=0, and active GameplayRestriction.NoCombat and GameplayRestriction.NoZooming. Removing both records in CET restored weapons, cyberware, scanner and Radioport without manually changing the player state machine.

The mod now matches that signature for eight consecutive one-second gameplay samples. It also refuses recovery if other GameplayRestriction-tagged records, PhoneCall, BlockFastTravel, FastForward, InDaClub or VehicleScene tags are present. It removes only the confirmed pair, once, then rearms after ten clean normal gameplay samples or a player/session change. It never changes quest facts or directly resets player or radio state.

## Test

- Install the game-root artifact (replace the old REDscript file); keep a backup of the original affected save.
- Load the original broken save before any CET repairs. Do not enter combat or use CET to clear effects. Wait at least eight gameplay seconds.
- Verify weapon drawing, cyberware, scanner zoom and Radioport.
- On a healthy save test dialogue, phone calls, safe zones, scripted quests and vehicles. Confirm intentional restrictions remain.
- Uninstall by deleting `r6/scripts/ActionBlockedFix`.

## Limitations

The repair itself was confirmed via CET on one affected save; automatic REDscript detection and the new API calls still require compilation and runtime testing. The guards narrow the reproduced signature but cannot prove which quest or native system owns a restriction. Untagged phone/quest locks may not be detected. Other restriction combinations, zones and combat states are deliberately not repaired. Do not publish as stable until positive and negative gameplay tests pass.

Reference for the vanilla zone/fact/tag checks: https://github.com/spuddeh/cp2077-radio-xl/blob/main/r6/scripts/RadioXL/Restrictions.reds
