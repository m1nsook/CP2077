# Delamain AV — 0.1.0-dev.2

Stationary passenger-only prototype for Cyberpunk 2077 2.50. Uses the original quest entity via the separate `Vehicle.m1nsook_delamain_av` record. Does not register an owned vehicle or modify the ending.

## Requirements

RED4ext, redscript, TweakXL, M1 Arc 1.0.1 or newer (1.0.10 recommended), Codeware (temporary spawning backend). DAV and CET are not required. Disable DAV for isolation. No custom archive or DLL is shipped.

The archive has a development suffix; the manifest must remain numeric `0.1.0` for M1 Arc discovery. Fully restart the game after replacing the files.

## Test

Load a disposable save in a flat, open empty area, with V on foot. Face at least 12 metres of clear ground. Open M1 Arc > Gameplay > Delamain AV. Use **Spawn test AV**, close the overlay, and check the vehicle exterior. Approach it, then **Open passenger door** and **Board as passenger**. Close the overlay to observe the boarding animation. Use **Inspect vehicle** and copy the M1 Arc diagnostics. Exit normally before **Remove test AV**.

If the model falls, moves unexpectedly, or has no safe seat, stop testing and reload the save. This is NOT an autonomous taxi yet. No takeoff or flight is implemented. Don't save while inside the test vehicle. The spawn position is a simple forward offset, not a validated landing location. The actual quest entity's rear seat and animation support require in-game confirmation.

## Research

- DAV 3.5.0 uses Codeware's DynamicEntitySystem for entity lifecycle and a native FlyAVSystem for flight. Upstream [repository](https://github.com/tidusmd0806/CP2077-DriveAerialVehicle) main at c318330ce610e61f3350c0a723de00db4fbee4bb includes the binary/interface and Lua/REDscript, not the C++ implementation. No upstream code or binary redistributed here.
- Codeware [DynamicEntitySystem](https://github.com/psiberx/cp2077-codeware/blob/main/scripts/World/DynamicEntitySystem.reds) supplies the temporary spawning backend.
- Original game record and mounting API confirmed against provided 2.50 exports.
- Next: verify spawning and boarding, then implement a scoped native controller and safe flight tests. Passenger-only; no manual flight controls. Remove Codeware dependency only when a replacement is verified.
