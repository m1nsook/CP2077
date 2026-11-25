# Kineticle

Native development has begun in [Native](Native/README.md). The native dev.6 probe is diagnostics-only; the tested dev.5 REDscript implementation below is retained as a fallback. They are separate builds, not interchangeable controllers.

Experimental collision force amplification and vehicle protection for vehicles driven by V.

## 0.1.0-dev.5

User testing of dev.4 confirmed that collision enhancement has an effect and other vehicles can be damaged again. Dev.5 retains the same protection and force API, expands the multiplier from 1–20x to 1–100x, raises the two experimental caps to 100 estimated delta-V and 300,000 native units, and records requested versus capped force. The caps can still cause saturation; 100x is not a guarantee of 100x physical impulse. Start at 20x and increase gradually on a disposable save. Compare the same car and target at 1x, 20x, 50x and 100x. Report after each comparison.

### Existing mechanics

Install both updated ZIPs if using M1 Arc; overwrite the previous Core and controller. M1 Arc is optional (1.0.1+). NSUI and Mod Settings are planned. Load a disposable save made before dev.2/dev.3 was installed: those prototypes could have left their GodMode sources on previously driven vehicles. This version does not add or remove GodMode, nor does it modify shared vehicle stats or TweakDB.

- Damage protection checks the actual hit target against the live driver vehicle at damage dispatch, then zeroes computed damage only for that hit. No persistent god-mode flags or polling. Visual resets are likewise scoped to the driver vehicle. User testing confirmed this restores NPC vehicle damage.
- Collision force now uses the struck vehicle's native GetTotalMass() and bump event delta-V to estimate extra momentum. The previous measured force was zero on most collisions. The extra native force is bounded to 300,000 units and equivalent estimated delta-V of 100, with a 0.30s cooldown. The native AddCollisionForce() units and physical effect remain unknown.
- Logs record rejected events, applied force, target mass, projected velocity before and 0.10s after the call, protected hits, unchanged vehicle hits and visual resets. The velocity sample is observational; vanilla motion and later contacts can also affect it.
- Core and controller build versions are logged. Other M1 Arc setting changes no longer log redundant configuration applications.

Test on a clean disposable save. Compare 1x, 20x, 50x and 100x using the same vehicle and target. Confirm a never-driven NPC car can be damaged while V is driving and on foot; test the formerly driven vehicle after exiting; then test V's car and motorcycle. Report diagnostics separately after each test. The prior GodMode mechanism is not used in this build.

Native signatures were inspected in the provided 2.50 RTTI and the game script symbol names. The semantic compiler and in-game behavior still need confirmation. No tire or passenger protection, recoil suppression, or Core-owned persistence yet. The controller persists its own values through M1 Arc.
