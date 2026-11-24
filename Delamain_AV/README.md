# Delamain AV — 0.2.2-dev.9

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
