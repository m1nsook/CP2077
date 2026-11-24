# Delamain AV — 0.2.1-dev.8

dev.7 established the real failure: the original OccupantSlots component points to a passenger workspot with a looping seated sequence and no entry/exit nodes. The original spawn, 0.75 m clearance, exterior door animation, rear-left passenger PSM and despawn are already confirmed. Do not retest those as separate milestones.

## Native resource implementation

- `m1nsook\delamain_av\passenger.workspot` adds the original q113 player entry and exit animation names from q203_04_delamain.scene as workEntryAnim and workExitAnim with the native PSM's default slot. Preserves the original player rig/animation-set mappings and seated loop.
- `m1nsook\delamain_av\occupant_patch.ent` supplies only the exact OccupantSlots component (name and CRUID preserved), including its original seat transforms, with the custom passenger workspot reference. There is no driver slot data.
- ArchiveXL makes an independent copy of the vanilla q203 entity at the mod's private entity path and patches only that copy's occupant component. TweakXL points the existing mod vehicle record to the copy. No original resource or quest fact is overwritten.
- The real archive is compiled and round-tripped with WolvenKit CLI 10.0.0-beta.1. No original animation binary or other mod code is redistributed.
- Native exit still uses the passenger PSM and workspot. The redundant car-like exit-direction gate is removed for this AV; the actual workspot/navmesh check remains. No teleport fallback is added by this mod.
- Right rear interaction is disabled because the original entity defines only the left rear passenger slot.

## Requirements

Cyberpunk 2077 2.50, RED4ext, REDscript, TweakXL, Codeware, M1 Arc 1.0.1+ and ArchiveXL with resource.copy support (required for the private copied entity). No CET or DAV dependency.

## Validation

CI must compile the two custom resources, export them back to JSON and verify the nodes and component binding, then pack an install-ready archive and ZIP. It does not verify game animation playback, alignment, native PSM timing, or ArchiveXL's in-game copy/patch application. The cinematic scene tier and autonomous flight are not yet implemented. No change to main.
