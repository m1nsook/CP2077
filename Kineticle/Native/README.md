# Kineticle 0.1.0-dev.11-contact-motion

Core is a native RED4ext DLL. Optional M1 Arc controller is a separate ZIP. Dev.9's paired-motion experiment is removed from the active gameplay path, with its old pure-math tests retained for reference. No global TweakDB changes, mass changes, GodMode, or forced traffic activation. Physics writes are disabled by default.

## New motion approach

The pinned LTBF reference demonstrates the 2.50 rigid-body fixed-step hook `RigidBody::IntegrateMotion(float)` (hash 2021199885) and modifies the current body's velocities before vanilla integration (ChassisContacts.cpp). It also documents ApplyImpulse (611586815, offset + force), but the force accumulator's consumption point has not been verified for this use; no direct invocation of that ABI is made. Unlike dev.8/9, no paired sample matching or momentum-conservation gate is used. An exact VehicleBumpEvent pair authorizes a one-shot correction for each entity independently, based on its own observed sync delta and the direction from the player vehicle to the target. Traffic-state NPC measurements are allowed; only a naturally dynamic current rigid body may receive its queued correction. The first eligible per-body sample is recorded, with bounded post-event observation and lifetime windows (90/220 ms), a duplicate cooldown, no raw pointer persistence, no cross-worker writes, and no change to ordinary driving or vertical linear speed.

Session-only opt-in at M1 Arc: nominal outgoing horizontal correction 5x (extra capped at 12 m/s), forward recovery 75% (player horizontal addition capped at 8 m/s), lateral recoil and angular correction reduction 50% (angular addition capped at 3 rad/s). The target and player writes are independent. Exact contact-solver identification remains unverified: same-frame wall/ground contacts can contaminate the correction, and a delayed driver transition can occur before the controller catches it. The experiment must not be enabled by default or released. Event-time native road/driver/remote/quest/destroyed guards plus the existing controller PSM/tag sample authorize; the standalone native scope is attempted only without a controller sample. No explicit physics API has yet proved in-game delivery. The new fixed-step hook uses a reference-backed ABI; Windows compilation is not runtime validation.

The native report includes CONTACT_MOTION counters and up to 24 finished contact attempts. Check V_measured/NPC_measured vs V_writes/NPC_writes, traffic_measured, integrator_calls, guard_failed/direction_failed and expiry. Offline tests replay the actual dev.8 motorcycle/traffic correction independently and test duplication, invalid direction, traffic-only observation, delivery and stale context. The prior motion trial is retained only as offline regression/reference.

## Protection

Three independent opt-in controls: mechanical damage (1), body-grid reset (2), glass reset (4). Stored in Core's `red4ext/plugins/Kineticle/config.json` as `protectionFlags` bitmask (0–7); old format 1 configs without this field migrate with all protection off. Manual config changes apply on next launch; M1 Arc changes apply live. `probeEnabled` only controls read-only diagnostics. Core has no REDscript/CET/controller dependency.

The exact 2.50 RTTI export confirms gameDamageSystem::DealDamages;gameHitEvent, gameeventsHitEvent.target (weak gameObject), attackComputed (gameAttackComputed), GetAttackValues/SetAttackValues (array:Float), VehicleComponent.GetVehicle; (instance weak vehicleBaseObject), OnGridDestruction and DestructionResetGrid/Glass. All runtime function and property metadata is validated, no hardcoded member offsets. Damage values are zeroed for the current hit before vanilla DealDamages; readback distinguishes an actual zeroed array from a failed API. Grid/glass resets occur after the vanilla callback, as in the earlier REDscript implementation. Native delivery has NOT yet been confirmed in game.

Scope checks require native player driver and mounted state, road vehicle, no remote/quest/locked/destroyed state, local player and exact mounted vehicle, PSM Driving/DriverCombat, and all three restrictions absent (NoDriving, VehicleOnlyForward, VehicleScene). The native player-system/blackboard/Tag RTTI route is independent of the controller and fails closed if signatures or calls fail. No sampled controller context is used to authorize protection.

Grid observations report actual other-vehicle and ramming flags for future physics investigation; they are not treated as a collision solver hook. Tire/occupant/ejection/explosion pathways remain pending. Physics features above are experimental and not exact solver impulse scaling. Body and glass reset are restorative, not a guarantee against every destruction path.

## Test

Install both updated ZIPs, replacing the dev.9 controller. On foot reset diagnostics once, enable Experimental contact motion, enter a car and ram several vehicles (including moving traffic), then hit a wall separately. Repeat with a bike if safe in game, exit, disable the experiment and report. Protection settings are independent; leave them unchanged unless desired. Check that NPCs and vehicles not driven by V still take damage. Report once and send diagnostics.txt. PROTECTION and GRID counters distinguish runtime scope failures, API failure, verified hit zero, and visual reset calls. Do not extract further game data. No merge/release before in-game verification.

## Verification

Windows CI compiles the DLL and runs pure C++ policy/replay tests; the optional controller is parsed, not fully game-compiled. Real game test is necessary for native player system calls and damage delivery. The package remains two separately installable game-root ZIPs.
