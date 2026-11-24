# Delamain AV — 0.2.0-dev.7

Stationary native passenger transition experiment for Cyberpunk 2077 2.50. Requires RED4ext, REDscript, TweakXL, Codeware and M1 Arc 1.0.1+ (1.0.10 recommended). No CET or DAV dependency. No original quest resources or facts changed. Numeric manifest version 0.2.0.

## Test focus

Use a disposable save, flat empty ground, 0.75 m clearance. Spawn the AV, approach the physical left entrance, open it, board from M1 Arc, close the overlay and observe. Inspect the AV after boarding; look for PSM_PASSENGER, WORKSPOT, WORKSPOT_PHASE and WORKSPOT_EXIT. Request Exit as passenger only through M1 Arc for this test. If EXIT_BLOCKED or EXIT_TIMEOUT appears, reload the test save rather than attempting other exit controls. If successful, inspect the exterior before removing. Never save while mounted.

The mount request now preserves position as vanilla mounting does. Door closing waits for the passenger PSM. Exit uses the native Delamain PSM exit request only after verifying the mounted workspot, exit nodes, native exit direction, stationary position and open physical door. No mod teleport, no direct low-level unmount. Native game code may still choose a fallback depending on geometry. Do not interpret an unmount as proof of cinematic animation or absence of damage.

## Research from 2.50 exports

- `gameVehicleMountableComponent.script`: MountEntityToSlot uses preservePositionAfterMounting=true.
- `vehicleTransition.script`: EnteringEvents uses MountToVehicle with OccupantSlots and default entry slot. PassengerEvents calls HandleExitRequest, which consumes VehicleComponentPS.ShouldForceExitDelamain and sets validExitRequest. ExitEvents handles low-level unmount after exiting. The native PSM can still use its own teleport fallback; this prototype does not override it.
- `workspotSystem.script`: runtime workspot functions operate on existing entity containers, slots, actors and animation names, not arbitrary external .workspot resource paths.
- q203_04_delamain.scene contains scnWorkspotData_ExternalWorkspotResource for `base/workspots/quest/main_quests/part1/q113/q113_05_av_flight/player__sit_av_excalibur__sit_around__01.workspot`, instance 674, entry 2, mounted scene workspot, Tier4_FPPCinematic and separate cinematic player/vehicle animations. The supplied Excalibur gameplay/base.workspot is a different resource (door-closed idle).
- q203 scene movement uses questMoveOnSpline_NodeType and named world NodeRefs. AIVehicleOnSplineCommand also takes a NodeRef and inherits useKinematic/needDriver. No standalone runtime-generated spline or valid independent world node has been verified. Do not drive the original quest splines or mutate original quest state.
- Missing exports: `base/quest/main_quests/epilogue/q203/characters/q203_delamain_av.ent` and the external player workspot above, both raw and WolvenKit JSON. These are needed to establish existing component bindings and author a separate custom resource. No custom scene is shipped yet.
- DAV and AeroMobility references are not included or required.

## Validation

The packaging workflow performs static source and archive checks only, not REDscript compilation. The cinematic entry/exit assets have not yet been integrated. Flight is not implemented. Main remains untouched.
