# Delamain AV — 0.1.3-dev.5

Stationary passenger-only prototype for Cyberpunk 2077 2.50. Uses the original quest entity via the separate `Vehicle.m1nsook_delamain_av` record. Does not register an owned vehicle or modify the ending.

## Requirements

RED4ext, redscript, TweakXL, M1 Arc 1.0.1 or newer (1.0.10 recommended), Codeware (temporary spawning backend). DAV and CET are not required. Disable DAV for isolation. No custom archive or DLL is shipped.

The archive has a development suffix; the manifest must remain numeric `0.1.3` for M1 Arc discovery. Fully restart the game after replacing the files.

## Test

Load a disposable save in a flat, open empty area, with V on foot. Face at least 12 metres of clear ground. Open M1 Arc > Gameplay > Delamain AV. Use **Spawn test AV**, close the overlay, and check the vehicle exterior. Approach it, then **Open passenger door** and **Board as passenger**. Close the overlay to observe the boarding animation. Observe the physical front-left entrance, while V remains rear-left. Close entrance is also available. Use **Inspect vehicle** to confirm the rear seat. Use **Exit as passenger** for a controlled stationary unmount and return to the position captured before boarding. Do not move the AV. Remove only after exiting. Copy diagnostics. Native entry/exit animations are not guaranteed; the fallback is intentionally instant. Test the visuals, not just the logical door state.

If the model falls, moves unexpectedly, or has no safe seat, stop testing and reload the save. This is NOT an autonomous taxi yet. No takeoff or flight is implemented. Don't save while inside the test vehicle. The spawn position uses a static terrain raycast and adjustable origin clearance (default 0.75 m). This is not a full landing-site validator and does not control gravity. Initial and settled height are logged; M1 Arc boarding is blocked if clearance is unstable. Do not use native Get in in this build. Test spawn clearance before any boarding. If clipping persists, remove the unoccupied vehicle and send diagnostics. The actual quest entity's rear seat and animation support require in-game confirmation.

## dev.4 test focus

User reports showed that the physical door and automatic closing work through M1 Arc, while native Get in bypasses that path. Both showed cabin clipping into the road. DAV's uploaded 3.5.0 reference uses a 1 m minimum ground clearance and disables gravity via its native controller. This build measures the origin height without using that DLL; it does not claim to solve gravity, camera or native animation issues.

## dev.5 test focus

Dev.4 returned impossible height values despite a visually stable AV. Sample native position into a typed local before extracting coordinates; log raw Z and surface Z separately and reject impossible coordinates as invalid rather than drift. Boarding still requires a valid clearance measurement. Default 0.75 m follows the user's visual observation. Inspect now reports actual passenger PSM and high-level state. Do not force PSM or scene tier; no status effects are applied while parked. The final boarding flow should follow quest/dialogue-style interaction rather than standard driver controls. Exact q203 scene/quest graph wiring remains unverified; do not modify original ending resources. No flight in this build.

## Research

- DAV 3.5.0 uses Codeware's DynamicEntitySystem for entity lifecycle and a native FlyAVSystem for flight. Upstream [repository](https://github.com/tidusmd0806/CP2077-DriveAerialVehicle) main at c318330ce610e61f3350c0a723de00db4fbee4bb includes the binary/interface and Lua/REDscript, not the C++ implementation. No upstream code or binary redistributed here.
- Codeware [DynamicEntitySystem](https://github.com/psiberx/cp2077-codeware/blob/main/scripts/World/DynamicEntitySystem.reds) supplies the temporary spawning backend.
- Original game record and mounting API confirmed against provided 2.50 exports.
- Next: verify spawning and boarding, then implement a scoped native controller and safe flight tests. Passenger-only; no manual flight controls. Remove Codeware dependency only when a replacement is verified.
