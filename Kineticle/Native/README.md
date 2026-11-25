# Native Kineticle (dev.6 probe.5)

Probe.2 added read-only observation of `OnVehicleBumpEvent` through the pinned SDK's `CBaseFunction::ExecuteScripted` signature and exact `CStack_vtbl` guard. It checks the reflected handle type and resolves the event's weak hitVehicle reference only within the callback. It records both entity IDs, delta velocity, hit normal, native IsPlayerDriver results and remote/quest/destroyed/car-or-bike checks. Missing or non-native Bool methods produce -1 (unknown). No pointer is retained and no RTTI is invoked from the physics callback.

Probe.2 observed six bump callbacks in-game, all rejected by the single CStack vtable guard. Probe.3 accepts the two dispatcher argument representations documented by [CET's native function observer](https://github.com/maximegmd/CyberEngineTweaks/blob/d7c378d4e9c7b15ca1ce42c37e8732d3b397b047/src/scripting/FunctionOverride.cpp#L153-L194). CET's CScript_RunPureScript hash matches the SDK's ExecuteScripted hash (3791200470). The decoder checks the reflected handle and return types, uses the stack's result accessor to distinguish array and packed arguments, and rejects unexpected counts, offsets and inaccessible storage. The original CStack vtable is retained as an additional check, not a universal requirement. The report counts each accepted layout separately. No CET runtime dependency. Event dispatch coverage, thread affinity, PSM state and the precise contact-to-sync timing remain unverified; the native guard counter is not permission to modify physics.

Standalone x64 RED4ext DLL. Core contains no REDscript and creates its own config.json on first load. The optional M1 Arc controller supplies only diagnostics and a probe toggle.

**This is not a gameplay replacement yet.** No impulses, velocity, torque, health, collision flags or mass are modified. The proven dev.5 scripts remain under ../Core and ../Controllers as a fallback; do not use the old and new controllers together. Remove the two old Kineticle REDscript folders and old M1 Arc manifest folder before testing the native-only packages. Do not overwrite your only working dev.5 archive.

The native physics entry point is vehicle::PhysXSynchronizer::SyncFromPhysX (RED4ext hash 547299556). References: https://github.com/jackhumbert/let_there_be_flight/blob/main/src/red4ext/Physics/ChassisContacts.cpp and the same project's pinned SDK fork 21efd4250f18f6703d6ff7caba0245093b834859. The hook is version-gated to executable 3.0.88.46235 and RED4ext 1.31.1.0. Missing addresses should leave it inactive. The player-controlled flag is a candidate signal, not authoritative driver/vehicle pair identification.

Probe.3 saw five bump callbacks and rejected all five before decoding. Probe.4 no longer requires the caller to provide a callback return buffer: the reference observer uses the return address for layout selection, not validation. Array arguments use reflected parameter metadata when the array type is absent (conflicting metadata is still rejected). Packed offsets must fit the reflected stack size when nonzero. All argument paths still require the exact reflected bump event handle type, readable storage and the actual event object's RTTI class before accessing event fields. Reports include the exact failed gate, layout discriminator, return-buffer presence, parameter offset and size, and cumulative rejection counts. This addresses plausible false rejections; the probe.3 aggregate counters did not identify which gate failed.

Probe.4 confirmed packed event arguments with discarded return values, two valid player/vehicle pairs and successful native IsPlayerDriver/remote/quest queries. IsDestroyed returned unknown. Probe.5 permits the reflected scripted zero-argument Bool implementation of IsDestroyed via the SDK execution API, still failing closed if the query cannot be made. It also checks the struck vehicle's destruction and quest states. A bounded numeric-only trace captures the strongest sync velocity changes on the exact two entity IDs within 150 ms before/after each bump callback; the four most recent player pairs are reported with signed timing, sequence, thread, velocity and angular-change data. Temporal matching alone does not prove a contact caused a sync correction; all gameplay writes remain disabled. The player PSM/status-effect checks and native impulse delivery remain open.

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
