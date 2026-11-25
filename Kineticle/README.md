# Kineticle

Experimental collision force amplification and vehicle protection for vehicles driven by V.

## 0.1.0-dev.3

Compiler compatibility fix: use the REDscript class name `VehicleGridDestructionEvent` and compare vehicle entity IDs instead of mixed reference types. Gameplay behavior is unchanged from dev.2.

Install the Core ZIP into the game root. M1 Arc controller is optional and requires M1 Arc 1.0.1+. Remove any previous KinetiCars probe scripts and manifest before installing. NSUI and Mod Settings are not implemented.

Core defaults: enabled, 5x measured collision force (extra force capped at 30,000 native units), invulnerability, collision deformation/glass reset, motorcycles included. No mass/handling/TweakDB edits. Only V's currently driven car or bike receives protection, and only the other vehicle receives extra force. Scripted driving-state restrictions are respected. The force path skips missing or invalid measured force, rather than inventing a unit conversion. A 0.30s cooldown bounds repeated calls.

Test on a disposable save. Start at 1x then try 5x, compare the same car/truck collision, report diagnostics in M1 Arc after each test. Try bullet damage and collision deformation separately, then disable protection and exit the vehicle. Report whether the other vehicle moves farther, whether V's vehicle takes damage, and any crashes. "force calls" counts native invocations, not confirmed successful impulses.

Native signatures checked against user-supplied 2.50 RTTI, runtime semantics not yet verified. God mode effectiveness and removal, visual reset, and force behavior require in-game testing. Tire damage, passenger injury protection, original recoil suppression, Core-owned persistence, and additional controllers are pending. M1 Arc persists its own settings; standalone Core uses defaults in this build. No full semantic compile without configured private game bundle; controller semantic linking additionally needs M1 Arc declarations.
