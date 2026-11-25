# Action Blocked Fix

Experimental automatic recovery for Cyberpunk 2077 2.5. Requires REDscript only.

## Recovery

Matches the reproduced normal-exploration NoCombat + NoZooming lock for eight consecutive gameplay samples, preserving the guards from dev.3. Removes only those two effects and lets vanilla recover the upper-body state and Radioport.

A callback records new applications of the two effects and resets the candidate wait. A ten-sample probation starts before removal. Any reapplication event, epoch change or live presence of either effect during probation locks out further repairs for the current player/session. After successful probation, ten clean normal gameplay samples are required to rearm. No persistent data, quest facts, native state overrides or extra logging dependencies.

## Test

- Install the development artifact over dev.3 and load the original broken save. Confirm automatic recovery after about eight gameplay seconds.
- Test normal play, dialogue, phone calls, braindance, radio scenes, safe zones, vehicles, scripted quests and fast-travel restrictions; no repair should occur.
- Optional temporary CET diagnostics (not a mod dependency): `local p=Game.GetPlayer(); print("ABF state:",p.misoRecoveryState,"fixes:",p.misoFixCount,"epoch:",p.misoRestrictionEpoch,"probation:",p.misoProbationActive,"reapplied:",p.misoReappliedDuringProbation)`
- State 0 = no fix yet; 2 = probation; 3 = passed; 4 = lockout. Fix count should stay zero during legitimate restrictions. During an affected-save test state should become 2 then 3.
- On a disposable test save, after state 2, apply NoCombat with CET within ten gameplay seconds, resume gameplay and check state 4. Confirm a new save/player session clears the lockout. Do not save the artificially restricted state.
- Remove `r6/scripts/ActionBlockedFix` to uninstall.

## Limitations

The dev.3 recovery was confirmed in game; this revision requires separate semantic compilation and testing. The application callback is supported by public game API examples, but has not been verified against the exact 2.5 bundle. The epoch is evidence of application, not ownership. A legitimate restriction matching every guard for eight seconds could still be removed once, and late reapplication beyond probation is not necessarily caught. A new legitimate restriction during probation also causes conservative lockout. No stable release until positive and negative tests pass.
