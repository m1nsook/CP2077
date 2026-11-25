# Native Kineticle (dev.6 probe.2)

Probe.2 adds read-only observation of `OnVehicleBumpEvent` through the pinned SDK's `CBaseFunction::ExecuteScripted` signature and exact `CStack_vtbl` guard. It checks the reflected handle type and resolves the event's weak hitVehicle reference only within the callback. It records both entity IDs, delta velocity, hit normal, native IsPlayerDriver results and remote/quest/destroyed/car-or-bike checks. Missing or non-native Bool methods produce -1 (unknown). No pointer is retained and no RTTI is invoked from the physics callback. Event dispatch coverage, thread affinity, PSM state and the precise contact-to-sync timing remain unverified; the native guard counter is not permission to modify physics.

Standalone x64 RED4ext DLL. Core contains no REDscript and creates its own config.json on first load. The optional M1 Arc controller supplies only diagnostics and a probe toggle.

**This is not a gameplay replacement yet.** No impulses, velocity, torque, health, collision flags or mass are modified. The proven dev.5 scripts remain under ../Core and ../Controllers as a fallback; do not use the old and new controllers together. Remove the two old Kineticle REDscript folders and old M1 Arc manifest folder before testing the native-only packages. Do not overwrite your only working dev.5 archive.

The native physics entry point is vehicle::PhysXSynchronizer::SyncFromPhysX (RED4ext hash 547299556). References: https://github.com/jackhumbert/let_there_be_flight/blob/main/src/red4ext/Physics/ChassisContacts.cpp and the same project's pinned SDK fork 21efd4250f18f6703d6ff7caba0245093b834859. The hook is version-gated to executable 3.0.88.46235 and RED4ext 1.31.1.0. Missing addresses should leave it inactive. The player-controlled flag is a candidate signal, not authoritative driver/vehicle pair identification.

## Test
1. Start the game with native Core and optional diagnostic controller, check startup log.
2. Reset counters while on foot, report.
3. Drive normally, then report; hit a wall and report separately; hit another vehicle and report separately.
4. Compare player-controlled candidate frames, forward-loss samples and last pre/post velocity. Check bump callbacks, rejected stacks, observed vehicle pairs, player driver pairs and last pair fields. Reset between tests; for a vehicle hit exactly one IsPlayerDriver result should be 1 and the other 0. Wall-only contact must not create a player vehicle pair. Test a bike and passenger/remote control when possible. No gameplay differences expected.
5. Event sync sequence is a counter snapshot, not a proven one-to-one link to a physics sample. -1 means unavailable, not false. If callbacks stay at zero or stacks are rejected, the dispatcher path has not been validated.

## Next implementation gates
- Establish native collision partner identification and authoritative V driver scope, excluding remote/scripted control.
- Verify the contact-specific rigid-body impulse path and selectively recover only forward collision loss. Offline policy has tests, but is not executed in the probe.
- Port the confirmed damage target guard into a native damage dispatch hook; verify NPC damage before enabling protection.
- Expand native config and optional controllers only for verified features.

No third-party flight DLL or CET is needed. Native build pins the reference SDK and uses the MIT-licensed nlohmann JSON library; SDK and JSON licenses are included by CI.
