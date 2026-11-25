# Native Kineticle (dev.6 probe.1)

Build correction: use the pinned SDK fork's actual v0 API aliases and runtime 2.50 constant. The physics hook and gameplay behavior are unchanged.

Standalone x64 RED4ext DLL. Core contains no REDscript and creates its own config.json on first load. The optional M1 Arc controller supplies only diagnostics and a probe toggle.

**This is not a gameplay replacement yet.** No impulses, velocity, torque, health, collision flags or mass are modified. The proven dev.5 scripts remain under ../Core and ../Controllers as a fallback; do not use the old and new controllers together. Remove the two old Kineticle REDscript folders and old M1 Arc manifest folder before testing the native-only packages. Do not overwrite your only working dev.5 archive.

The native physics entry point is vehicle::PhysXSynchronizer::SyncFromPhysX (RED4ext hash 547299556). References: https://github.com/jackhumbert/let_there_be_flight/blob/main/src/red4ext/Physics/ChassisContacts.cpp and the same project's pinned SDK fork 21efd4250f18f6703d6ff7caba0245093b834859. The hook is version-gated to executable 3.0.88.46235 and RED4ext 1.31.1.0. Missing addresses should leave it inactive. The player-controlled flag is a candidate signal, not authoritative driver/vehicle pair identification.

## Test
1. Start the game with native Core and optional diagnostic controller, check startup log.
2. Reset counters while on foot, report.
3. Drive normally, then report; hit a wall and report separately; hit another vehicle and report separately.
4. Compare player-controlled candidate frames, forward-loss samples and last pre/post velocity. No gameplay differences expected.

## Next implementation gates
- Establish native collision partner identification and authoritative V driver scope, excluding remote/scripted control.
- Verify the contact-specific rigid-body impulse path and selectively recover only forward collision loss. Offline policy has tests, but is not executed in the probe.
- Port the confirmed damage target guard into a native damage dispatch hook; verify NPC damage before enabling protection.
- Expand native config and optional controllers only for verified features.

No third-party flight DLL or CET is needed. Native build pins the reference SDK and uses the MIT-licensed nlohmann JSON library; SDK and JSON licenses are included by CI.
