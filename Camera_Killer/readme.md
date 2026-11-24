# Camera Killer 1.1.0

Turns off **other cameras connected to the same network** when V shuts down one surveillance camera. Tested in Cyberpunk 2077 2.5 (previously named *Miso Camera Spread*).

## Features

- Spreads standard Camera Shutdown quickhack, camera toggle, and network shutdown events to connected surveillance cameras.
- Prefers cameras linked through the same access point. If a camera has no direct access point, searches its surveillance-system or access-point ancestors.
- Sends normal in-game `SetDeviceOFF` events to connected cameras. Does not alter quickhack costs, hacking rewards, turrets, doors, or other devices.
- Excludes explicitly quest-marked cameras and avoids re-triggering itself from propagated shutdowns.

## Installation

1. **Remove the old version first:** delete `r6/scripts/MisoCameraSpread/`. Installing Camera Killer alongside Camera Spread would register the same behavior twice.
2. Extract the Camera Killer ZIP into your **Cyberpunk 2077** game folder.
3. Check that `r6/scripts/CameraKiller/CameraKiller.reds` exists, and launch the game.

**Requirements:** RED4ext, redscript and [M1 Arc 1.0.1 or newer](https://github.com/m1nsook/m1arc), compatible with Cyberpunk 2077 2.5. No CET, Codeware, TweakXL, ArchiveXL, or additional mod DLL is required.

Open Camera Killer in M1 Arc to enable or disable network propagation. The setting defaults to ON, persists through M1 Arc and applies to subsequent camera shutdowns. The Logs panel reports the effective setting. Turning the feature off does not undo previous camera shutdowns.

## Behavior and limitations

- The additional cameras receive an ordinary OFF action, not a timed quickhack. They can remain off after the original camera's quickhack expires.
- Cameras with no recognized access-point/surveillance master are unaffected.
- Network isolation and the behavior around special quest cameras should be checked when using other camera or security-network mods.

## Uninstallation

Delete `r6/scripts/CameraKiller/` and `red4ext/plugins/m1arc/mods/m1nsook.camera-killer/` and restart the game. Cameras already switched off during gameplay may retain their current state until normal game logic restores them or the area resets.
