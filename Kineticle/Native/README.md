# Kineticle 0.1.0-dev.12-native-mass

This development build replaces all dev.11 velocity writes with a vanilla mass mechanism experiment. No NPC-body writes, no integration hook, no per-bump RTTI, no TweakDB changes.

The pinned LTBF SDK documents RigidBody::SetUsesAlternativeChassisMass(float) and SetUsesNormalChassisMass using older executable opcode signatures. Both exact opcode sequences (except relative jump) must occur uniquely in the 2.50 executable and jump to the same executable update routine. If not, activation is refused. No direct field-write fallback. This is a runtime signature check, not proof of native function behavior.

The optional controller supplies exact mounted entity ID and fresh driver, road, quest, remote, destroyed and PSM/tag checks. Current physics callback must match that ID and body, player-controlled flag and fully dynamic state. The game setter is invoked once on that body's sync callback, with original chassis mass times 2–25, max 100000 kg, and readback. 1 is vanilla. Active modifications are restored on the next sync for that exact body on exit, stale scope, state transition, switch or disable. A different body is never written while an old modified body is still awaiting restoration; if it despawns before a sync, changing another vehicle is blocked until restart. Normal-mass setter restores normal mode, not necessarily the unused alternative value. No raw stored pointer is dereferenced. This experiment requires M1 Arc context; Core's protection remains independently native. Not ready for standalone mass release. No proof yet of collision-only mass: wall impacts and handling may change.

The report includes MASS: signature matches, RVA and common tail, requested multiplier, selected/active ID, apply/restore/failure counts, baseline chassis/total/inverse mass and current alternative/chassis/total/inverse readback. Native mass setter and thread context are not in-game verified. Disable the experiment immediately if instability occurs; restart the game if restoration fails. Do not hot-unload the DLL while mass is active. No new asset exports required.

## Test
Install both updated packages. Keep body/grid and glass restoration off to isolate the mass experiment. Reset diagnostics on foot, set mass multiplier to 5, drive a regular car into traffic, compare with 1, then exit and generate report. Try 10 only if 5 works safely. Confirm NPC-to-NPC collisions away from V remain vanilla. Report whether handling, stutter, model warping and wall behavior change. If the switch returns to 1 with a signature error, report once; do not retry. No merge/release before runtime validation.

## Verification
Windows CI compiles DLL and pure C++ policy tests; controller syntax parse is not a full game compile. Both packages remain separately installable.
