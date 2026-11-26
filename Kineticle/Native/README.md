# Kineticle 0.1.0-dev.15-scope-recovery

Standalone Core requires only RED4ext 1.31.1 and game executable 3.0.88.46235. The optional M1 Arc controller reads/writes the native configuration, reports diagnostics, and temporarily supplies the proven dev.13 driver scope bridge. Native-only authorization is separate and remains unverified. Do not install old REDscript Core packages alongside the native DLL.

## Configuration
`red4ext/plugins/Kineticle/config.json` (edit with game closed):
```json
{"format":2,"enabled":true,"massMultiplier":5,"includeMotorcycles":true,"protectionFlags":0}
```
New installations default to mass 1 and protections off; old format 1 is migrated atomically, preserving master switch and protection flags, with mass 1 and motorcycles enabled. 1–100x capped at 100,000 kg, the vanilla alternative mass value. Protection bits: mechanical 1, grid 2, glass 4. Sum enabled bits. Master switch disables all effects without clearing selected settings. JSON settings persist across sessions; invalid files fail closed. No live file watching. Optional controllers call native get/set functions. During this regression recovery, the installed M1 Arc controller also supplies driver scope using the proven dev.13 checks. Once claimed, stale/negative controller context fails closed; native fallback never overrides it. M1 Arc mass changes are debounced.

## Native mechanism
Preserves dev.13's executable timestamp, resolved addresses, byte checks, optional mass fields, native refresh and command observer. No NPC mass writes, shared TweakDB changes, integration hook, or synthetic velocity writes. The RED4ext Running callback samples local player and exact mounted vehicle on the game update thread every 100 ms, using driver, mounted, road type, remote, quest, destroyed, locked, PSM and tag checks. Physics sync only uses fresh authorization (250 ms), exact entity/body/chassis identity, player flag and dynamic state. Restore original alternative value and normal chassis mass on matching body sync after dismount/disable/transition. If a prior modified body disappears before restoration, new modification is blocked until restart. A failed native command or external change latches mass unavailable until restart, preserving saved requested configuration. No hot unload. Collision response against environmental objects may change; no claim of vehicle-only contact filtering.

## Verification
dev.13 user-confirmed native mass: 15 applies, 15 restores, 30 commands, no command/readback/restore failures. dev.14 regressed in-game at 25x. dev.15 restores dev.13 controller authorization, keeps native-only scope separate, and preserves persistent settings. The exact dev.14 failure is not proven without its diagnostic report. Native mechanical callback unverified; grid/glass existing implementation optional, off by default. Tire/occupant protection and NSUI/Mod Settings controllers deferred to later builds. Old impulse and contact tracing code excluded from runtime and active tests.

## Test
Install both matching packages. Use the M1 Arc UI (no manual JSON edits required). Compare 25x to dev.13 in the same car, with grid/glass restoration off. Report while driving and again after exiting. CORE_SCOPE and CONTROLLER_SCOPE are separate; MASS must show a positive apply count, effective last_applied_mass, and matching restoration. Standalone mode remains experimental until independently validated. Do not merge/release before runtime confirmation.
