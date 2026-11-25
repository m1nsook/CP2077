# KinetiCars

Collision impulse amplification and vehicle protection for vehicles driven by V.

**Development only — 0.1.0-dev.1.** The initial build observes collision events. It does not amplify impulse or prevent damage yet. Do not publish it as a functional gameplay mod.

## Packages

- Core: REDscript; no settings framework required.
- Controller — M1 Arc: Core and M1 Arc 1.0.1+.
- Controller — NSUI: planned; Core, CET and NSUI.
- Controller — Mod Settings: planned; Core and Mod Settings.

The Core will own the gameplay policy, validation and persistent configuration API. Controllers will only adapt the UI to that API. Use one controller at a time until cross-controller synchronization is implemented. The current probe toggle is temporary and stored by M1 Arc, not a finished Core settings implementation.

## Probe test

Install Core alone or Core plus the optional M1 Arc controller into the game root. The probe never changes game physics, damage, shared TweakDB records or quest state. Test in ordinary free roam on an expendable save.

1. With M1 Arc, enable Collision probe, then reset counters.
2. Drive a small car into a parked car and a moving truck. Report counters in M1 Arc Logs / Copy Diagnostics.
3. Let traffic hit V's car. Report again.
4. Repeat on a motorcycle, in a stolen car and while a passenger. Passenger and on-foot cases should not increment counters.
5. Disable the probe and repeat. Counts should remain unchanged.
6. Repeat after loading a save and after exiting/re-entering the vehicle.

Counters distinguish the receiver and other vehicle in the event; they do not yet establish causal impact direction or unique contacts. Delta-V is not impulse. The callback has precedent in the public cp2077-wheel-mod source; the driver and state checks have precedent in Immersive Driving. Both still require verification on the user's actual 2.5 installation.

## Next gates

- Confirm callback delivery and direction, exact 2.5 RTTI and a safe per-instance native impulse application path.
- Prevent duplicate impulses, bound extra impulse and reject non-finite inputs; bypass vanilla mass, handling and shared records.
- Verify independent mechanical, visual, tire and collision injury protection paths; never suppress quest events broadly.
- Implement Core-owned persistent settings; default settings must work without any controller.
- Add separate M1 Arc, NSUI and Mod Settings adapters, with compatible version checks.
- Test vanilla behavior when disabled, NPC-only collisions, scripted drives, dismount, save transitions and modded vehicles.

References: https://github.com/clevergrant/cp2077-wheel-mod/blob/main/gwheel_reds/gwheel_events.reds ; https://github.com/ArthurHub/Cyberpunk2077-Immersive-Driving/blob/main/scripts/System.reds ; https://github.com/m1nsook/m1arc/blob/main/docs/MOD_MANIFEST.md
