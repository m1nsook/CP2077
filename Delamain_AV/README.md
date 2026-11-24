# Delamain AV — 0.2.3-dev.10

dev.7 established the real failure: the original OccupantSlots component points to a passenger workspot with a looping seated sequence and no entry/exit nodes. The original spawn, 0.75 m clearance, exterior door animation, rear-left passenger PSM and despawn are already confirmed. Do not retest those as separate milestones.

## Native resource implementation

- `m1nsook\delamain_av\passenger.workspot` adds the original q113 player entry and exit animation names from q203_04_delamain.scene as workEntryAnim and workExitAnim with the native PSM's default slot. Preserves the original player rig/animation-set mappings and seated loop.
- `m1nsook\delamain_av\occupant_patch.ent` supplies only the exact OccupantSlots component (name and CRUID preserved), including its original seat transforms, with the custom passenger workspot reference. There is no driver slot data.
- ArchiveXL makes an independent copy of the vanilla q203 entity at the mod's private entity path and patches only that copy's occupant component. TweakXL points the existing mod vehicle record to the copy. No original resource or quest fact is overwritten.
- The real archive is compiled and round-tripped with WolvenKit CLI 10.0.0-beta.1. No original animation binary or other mod code is redistributed.
- Native exit still uses the passenger PSM and workspot. The custom resource uses cinematic root motion without navmesh snapping; the stationary/ground/structural workspot checks remain. Record-scoped PSM wrappers use the default exit animation and suppress road-car teleport recovery. No teleport fallback is added by this mod.
- Right rear interaction is disabled because the original entity defines only the left rear passenger slot.

## Requirements

Cyberpunk 2077 2.50, RED4ext, REDscript, TweakXL, Codeware, M1 Arc 1.0.1+ and ArchiveXL with resource.copy support (required for the private copied entity). No CET or DAV dependency.

## Validation

CI must compile the two custom resources, export them back to JSON and verify the nodes and component binding, then pack an install-ready archive and ZIP. It does not verify game animation playback, alignment, native PSM timing, or ArchiveXL's in-game copy/patch application. The cinematic scene tier and autonomous flight are not yet implemented. No change to main.

## dev.9 changes

dev.8 game test verified animated boarding both manually and through M1 Arc, physical door animation, auto-close after M1 Arc boarding, spawning and despawn. Structural exit nodes are now present, but the navmesh-checked query rejected the cinematic exit. The game also attempted an upward exit and left the player in a vehicle state.

- Keep entry resource and transforms unchanged.
- Disable road navmesh requirements and Z snapping on the custom exit node only; retain stationary landing and structural workspot guards.
- Route the native Exit input to the same door/exit request as M1 Arc. Use the native PSM default cinematic exit, suppress its car-specific teleport recovery only on the mod's private AV record.
- Remap rear-left door open/close events to the physical front-left entrance only for this record. Observe manual passenger mounting and close the door after entry.
- Native ExitEvents still owns the mounting relationship and player-state cleanup. No manual teleport or forced unmount fallback.
- Runtime alignment, successful unmount, and REDscript compilation remain unverified by CI.

## dev.10

dev.9 manual entry opens the physical door, plays the animation, and closes it automatically. Do not change the confirmed entry path. Exit logs show native mount and player-state cleanup in the same second as the request, not actual animated exit.

The supplied q203_04_delamain.scene JSON has a 5000 ms scnPlaySkAnimEvent for sit_couch__AV_excalibur__01__to__stand__2h_on_sides__01__turn270__01, with root motion enabled and scnUnmountEvent at 4999 ms. It is not a standard vehicle exit-slot sequence.

Use the imported WorkspotGameSystem.SendSlowExitSignal(actor, animationName, dontWaitForCurrentAnimationToFinish) instead of UnmountFromVehicle inside the custom record's ExitWorkspot wrapper. The native ExitingDecisions waits for the actor's workspot to end; native ExitEvents then cleans up the mounting relationship. Log the named exit entry ID, observed exiting phase, elapsed time, and displacement. Keep the entrance open on premature detach. Do not claim gameplay success solely from cleared mount or PSM. The 2.5 s and 1 m thresholds are diagnostic heuristics, not proof of exterior clearance.

No asset, entry, spawn, or original quest changes. The named workspot signal is imported in the supplied 2.50 script dump but its behavior on this AV remains unverified.

## dev.11 — AV-owned disembark sequence

Supersedes the dev.8–10 vehicle-exit-node approach. Original q203_04_delamain.scene uses a five-second skeletal animation followed by a separate unmount event at 4999 ms. The vanilla AV record inherits generic four-wheel data, but the cinematic sequence is not a road-car exit. The supplied SceneSystemInterface does not expose a standalone scene-start method.

Preserve confirmed manual boarding and physical door routing. In the private passenger workspot, replace the workExitAnim with a finite workAnimClip (entry ID 6) carrying the exact original Excalibur clip. workAnimClip type, flag 2 and blendOutTime are verified against WolvenKit RED4 generated classes. Native SendJumpCommandEnt is present in the supplied workspotSystem.script and is used by vanilla takedown sequences.

AV controller opens the entrance, jumps directly to this clip while V remains a passenger, waits for natural workspot end and checks elapsed time and displacement, then requests native PSM cleanup. The standard vehicle exit path is not responsible for animation or navigation. No mod teleport or forced instant unmount. The threshold checks are conservative heuristics; runtime clip playback, root motion, exterior position and cleanup still require verification. Forced engine transitions outside this managed lifecycle retain stock behavior.

Not a standalone cinematic scene yet: the source does not establish a safe script API for starting one. No original quest edits or new dependencies.

## dev.12 — user-provided missing animation-set correction

The original q203_04_delamain.scene resource's cinematicAnimNames[1] lists
sit_couch__AV_excalibur__01__to__stand__2h_on_sides__01__turn270__01.
cinematicAnimSets[1] points to:
`base\animations\quest\main_quests\prologue\q000\q000_corpo_03b_fly_to_lizzies\body\q000_corpo_03b_fly_to_lizzies__player_male.anims`

Entry and idle belong to q113, but exit belongs to q000. Add that exact
resource to both player rig cinematic setups and loadingHandles, priority 128,
preserving existing q113 entries and setup hashes. Verify this in the compiled
resource round trip. No change to entry/idle or original quest files.
Integrate the user's 4-second timed cleanup and stock-exit suppression. Cleanup
is not evidence of motion; leave the entrance open if displacement is below
1 m. If the clip still fails, detached V may remain inside the AV. No teleport
fallback. The actual .anims clip and hash computation are not independently
verified; runtime playback still requires game testing.

## dev.13 — proper workspot release with the now-verified q000 asset

dev.12 runtime confirmed the original exit animation plays but world displacement
is zero and the workspot remains active. Vanilla ExitingDecisions.ExitCondition
waits for IsActorInWorkspot to become false. Suppressing ExitWorkspot in dev.12
therefore left V stuck in the exit transition, including camera/movement.

Preserve the q000 resource correction, existing hashes, entry, seat and doors.
Restore a private workExitAnim (ID 6, default slot, no road-navmesh snap) now
that its actual animation set is included. Use the engine's non-instant
UnmountFromVehicle with that private node rather than a pose jump plus timer.
Check GetIdOfExitAnim against the resource's authored entry ID. Clear the
VehiclePassenger.isCar flag for the private AV exit request. Keep the
record-scoped suppression of road-car teleport recovery. Never use a manual
teleport or forced instant detach. ExitEvents owns cleanup. This is a native
workspot release, not a standalone cinematic scene.

Capture real animation name and entry ID from Codeware GetWorkspotInstance
(verified in Codeware v1.21.4), workspot phase, mount, vehicle PSM, position
and displacement every 0.5 s during exit. Codeware version must provide that
API. Door only closes after native state release and observed movement.
The 1 m displacement is a diagnostic heuristic, not proof of exterior clearance.
The intended motion and runtime cleanup are not yet verified.

## dev.14 — native taxi pickup and automatic entrance

Preserves dev.13 workspot and exit animation unchanged. The native summon popup
gets a separate top-row Delamain AV, with the vanilla cab's icon/style. No
garage ownership, original cab registration, or quest facts. Selecting it
requests the independent dynamic AV. On clear ground it spawns 6 m above
pickup and descends over roughly five seconds using Codeware Entity.SetWorldTransform.
This is an experimental unoccupied transform-based descent, not full flight
physics or a validated obstacle planner. The centre overhead ray is not a
complete vehicle-volume clearance check. If actual position diverges, abort
and keep boarding disabled. M1 Arc retains only stationary diagnostic spawn,
status, remove and clearance. No manual door actions. Entrance opens on
verified arrival/approach, closes after mounting/exit, and does not reopen
until V moves away. Exit requires a live raycast and verified parked position.

Vanilla map integration research: UISystem.RequestDelamainTaxiMenu opens the
existing taxi map. WorldMapMenuGameController.UpdateTravelDestination queues
SetTravelDestinationRequest(mappinID,cost); MappinSystem.GetMappin(id) and
IMappin.GetWorldPosition can resolve the destination. StartDelamainTaxiRequest
normally starts road AutoDrive via DelamainTaxiComponent and must be scoped
to the AV instead. Do not open the map until occupied flight and destination
landing are implemented; the stock taxi must remain unaffected.

Actions uploads the staged installation directory directly (one ZIP).
No archive or passenger animation changes. New UI, transform motion and
ground checks require runtime verification; no occupied ride yet.

## dev.15 — preserve ground-origin mounting and restore diagnostic exit

dev.14 confirmed the summon UI and arrival transform, but the original spawn
height was restored on boarding (+6 m). A successful visual transform does
not guarantee a relocated vehicle's authoritative mount origin. Preserve
dev.13's verified ground-origin passenger lifecycle: the empty, non-boardable
arrival AV is replaced at the end of descent by a newly created ground-origin
AV with the same record and orientation. This is a temporary two-entity
handoff, not a flight-physics solution. There may be a visible pop during
replacement; it requires runtime verification. MountEntityToSlot now checks
the tracked entity and live landing state, rather than relying on interaction
state alone. No occupied transform, despawn, teleport, or forced exit.
Restore a diagnostic disembark button using the same landed-only exit path.
Log a failed post-mount ground check once rather than flooding the log.

Use the vanilla VehicleQuestAVThrusterEvent to drive persisted thruster
state, and exact original q203 exterior landing and entrance audio event
names. Sound playback remains unverified. The original quest dust effect
is a separate scene resource; no guessed FX API or dust injection. Do not
enable occupied flight or destination travel until this pickup lifecycle
is proven. Single-ZIP Actions artifact.

## dev.16 — native physics descent experiment

Source reference: [tidusmd/CP2077-RED4EXT-DAV](https://github.com/tidusmd0806/CP2077-RED4EXT-DAV), commit 9f0d1359a7f5bb02d71ac6fec86c36aef9dd5615 (MIT). The supplied DAV 3.5.0 DLL corroborates the force/torque/wake relocation hashes and rigid-body owner/id offsets. The public DAV source's forked BaseObject size (0xBA0) differs from current official SDK (0xBC0); we do not use that layout or physicsData pointer offset. The bridge uses the engine callback's rigid-body pointer, checks its owner pointer and full EntityID, AV class, private record, exact executable file version 3.0.88.46235, finite/bounded velocity and mass, and a 1.5-second command lease. It preserves original callbacks and gravity, applies bounded velocity changes in matching callbacks, and uses the original physics wake function once on the game thread. No strong vehicle references or indefinite hook retries. The reverse-engineered velocity, angular velocity and mass offsets still require runtime validation; successful C++ compilation is not proof of ABI compatibility.

The scripted pickup now requests a downward velocity and observes actual world position rather than writing transforms. If the bridge is unavailable, has no matching callbacks, faults, or the empty AV drifts outside the narrow pickup corridor, abort and remove only the unoccupied arrival AV. No transform fallback, no occupied flight, no new UI. After actual descent to within 0.15 m, retain the dev.15 ground-origin handoff and the proven passenger system. This remains a prototype and the handoff can still visibly pop. No game testing in CI.

## dev.17 — DAV develop physics tick

Reference pinned to [develop commit 28a82418c9f956d2b3b06cdff575adc5a3752d63](https://github.com/tidusmd0806/CP2077-RED4EXT-DAV/blob/28a82418c9f956d2b3b06cdff575adc5a3752d63/src/Main.cpp#L1521), 19 commits ahead of main. The newer source drives velocity in VehicleApplyAllResistances (hash 2526549425) rather than the original force/torque hooks. It also reasserts physics when the vehicle is parked. Replace our two old hooks with this single physics-tick hook, pass the live vehicle handle to Pulse, wake only when the validated state is nonzero, and ramp the target velocity using actual engine deltaTime. Preserve the original resistance callback and gravity; no global force suppression. The fork's reciprocal physics/vehicle/body pointer fields are checked before accessing velocity; code -11 means pointer/state validation rejected, no write. Current SDK layout differs in overall size, so these offsets and the original tick signature remain runtime-unverified on 2.50. No occupied motion or transform fallback. The dev.15 ground-origin landing handoff, UI, door/audio and passenger system remain unchanged.

## dev.18 — physics-capable private Excalibur

The supplied DAV 3.5.0 entity export uses vehicleCarBaseObject, whereas q203_delamain_av uses vehicleAVBaseObject. Both use the same CarComponent and Chassis component IDs. Use ArchiveXL's supported entity-root replacement (ResourcePatchExtension::MergeEntity) on our private copy only; set both root entity and compiled package class to vehicleCarBaseObject, retain all original visual resources and our proven occupant patch. DAV's av.lua Spawn/SpawnToSky and engine.lua Init/UnsetPhysicsState/EnableGravity document the initialization sequence: bind after entity appears, wake physics, disable gravity, hold downward velocity, then zero velocity on landing. Wait for reciprocal physics pointer validation instead of rejecting immediately. Maintain zero-velocity hold on the same entity after landing; no replacement entity and no transform updates. The stationary M1 Arc spawn still uses the ground-origin path, now on the private car-based entity. Original quest untouched. C++/CR2W compilation cannot validate runtime physics offsets or changed car-class passenger behavior; those remain unverified. No occupied flight.

## dev.19 — complete the private car-class rig hierarchy

dev.18 crashed in game vehicle/chassis initialization before the native flight controller started. Comparison of the user-supplied DAV Excalibur entity export with the original q203 entity confirms that DAV adds an entAnimatedComponent named vehicle_rig (CRUID 1651752295586041860) using the vanilla v_base_x4_150_149_27 rig and animgraph, with AnimationController control binding. Its deformation_rig (CRUID 1651752295586041856) is controlled by and parented to vehicle_rig. Our class-only patch omitted both changes. Chassis collision resources and local transform are identical in both source entities. Add the missing vanilla rig and rebind the original deformation rig in the private ArchiveXL component patch, retaining the original Excalibur animset, invisible_in_fpp parameter and workspot. Use Vehicle.av_rayfield_excalibur as the private record base, as DAV's init.lua does, preserving the Delamain display name, Immortal tag, private q203-derived initial state and passenger-only actions. No copied DAV rig, mesh, app or archive. The flight controller is unchanged; these are necessary entity initialization corrections, not proof that car initialization or flight now works. The modified DAV deformation rig itself is not used, so equivalence of its bone transforms is not established. Runtime testing remains required.
