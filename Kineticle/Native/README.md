# Kineticle 0.1.0-dev.10-native-protection

Core is a native RED4ext DLL. Optional M1 Arc controller is a separate ZIP. Dev.9's paired-motion experiment is removed from the active gameplay path, with its old pure-math tests retained for reference. No physics writes, global TweakDB changes, mass changes, GodMode, or forced traffic activation.

## Protection

Three independent opt-in controls: mechanical damage (1), body-grid reset (2), glass reset (4). Stored in Core's `red4ext/plugins/Kineticle/config.json` as `protectionFlags` bitmask (0–7); old format 1 configs without this field migrate with all protection off. Manual config changes apply on next launch; M1 Arc changes apply live. `probeEnabled` only controls read-only diagnostics. Core has no REDscript/CET/controller dependency.

The exact 2.50 RTTI export confirms gameDamageSystem::DealDamages;gameHitEvent, gameeventsHitEvent.target (weak gameObject), attackComputed (gameAttackComputed), GetAttackValues/SetAttackValues (array:Float), VehicleComponent.GetVehicle; (instance weak vehicleBaseObject), OnGridDestruction and DestructionResetGrid/Glass. All runtime function and property metadata is validated, no hardcoded member offsets. Damage values are zeroed for the current hit before vanilla DealDamages; readback distinguishes an actual zeroed array from a failed API. Grid/glass resets occur after the vanilla callback, as in the earlier REDscript implementation. Native delivery has NOT yet been confirmed in game.

Scope checks require native player driver and mounted state, road vehicle, no remote/quest/locked/destroyed state, local player and exact mounted vehicle, PSM Driving/DriverCombat, and all three restrictions absent (NoDriving, VehicleOnlyForward, VehicleScene). The native player-system/blackboard/Tag RTTI route is independent of the controller and fails closed if signatures or calls fail. No sampled controller context is used to authorize protection.

Grid observations report actual other-vehicle and ramming flags for future physics investigation; they are not treated as a collision solver hook. Tire/occupant/ejection/explosion pathways and outgoing impulse/momentum/recoil/rotation are not implemented in this build. Body and glass reset are restorative, not a guarantee against every destruction path.

## Test

Install both updated ZIPs, replacing the dev.9 controller. On foot reset diagnostics once, enable the three protection toggles, enter a car, collide with another car and a wall, test gunfire against the vehicle, then exit. Check that NPCs and vehicles not driven by V still take damage. Report once and send diagnostics.txt. PROTECTION and GRID counters distinguish runtime scope failures, API failure, verified hit zero, and visual reset calls. Do not extract further game data. No merge/release before in-game verification.

## Verification

Windows CI compiles the DLL and runs pure C++ policy/replay tests; the optional controller is parsed, not fully game-compiled. Real game test is necessary for native player system calls and damage delivery. The package remains two separately installable game-root ZIPs.
