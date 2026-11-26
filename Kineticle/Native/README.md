# Kineticle 0.1.0-dev.14-standalone

Standalone Core requires only RED4ext 1.31.1 and game executable 3.0.88.46235. The optional M1 Arc controller only reads/writes the same native configuration and reports diagnostics. Do not install old REDscript Core packages alongside the native DLL.

## Configuration
`red4ext/plugins/Kineticle/config.json` (edit with game closed):
```json
{"format":2,"enabled":true,"massMultiplier":5,"includeMotorcycles":true,"protectionFlags":0}
```
New installations default to mass 1 and protections off; old format 1 is migrated atomically, preserving master switch and protection flags, with mass 1 and motorcycles enabled. 1–100x capped at 100,000 kg, the vanilla alternative mass value. Protection bits: mechanical 1, grid 2, glass 4. Sum enabled bits. Master switch disables all effects without clearing selected settings. JSON settings persist across sessions; invalid files fail closed. No live file watching. Optional controllers call native get/set functions; no controller owns driver authorization. M1 Arc mass changes are debounced.

## Native mechanism
Preserves dev.13's executable timestamp, resolved addresses, byte checks, optional mass fields, native refresh and command observer. No NPC mass writes, shared TweakDB changes, integration hook, or synthetic velocity writes. The RED4ext Running callback samples local player and exact mounted vehicle on the game update thread every 100 ms, using driver, mounted, road type, remote, quest, destroyed, locked, PSM and tag checks. Physics sync only uses fresh authorization (250 ms), exact entity/body/chassis identity, player flag and dynamic state. Restore original alternative value and normal chassis mass on matching body sync after dismount/disable/transition. If a prior modified body disappears before restoration, new modification is blocked until restart. A failed native command or external change latches mass unavailable until restart, preserving saved requested configuration. No hot unload. Collision response against environmental objects may change; no claim of vehicle-only contact filtering.

## Verification
dev.13 user-confirmed native mass: 15 applies, 15 restores, 30 commands, no command/readback/restore failures. dev.14 standalone update registration and persistence are not yet verified in game. Native mechanical callback unverified; grid/glass existing implementation optional, off by default. Tire/occupant protection and NSUI/Mod Settings controllers deferred to later builds. Old impulse and contact tracing code excluded from runtime and active tests.

## Test
1. Remove old optional controller (its exact REDscript file and manifest directory); install Core only. With game closed, edit config above to mass 5, protectionFlags 0. Launch, drive a car into traffic, exit, try another car, quit normally. Upload diagnostics.txt. Check CORE_SCOPE registered/running/allowed_samples and MASS applies/restores/last_applied_mass/last_restored_mass.
2. If standalone works, install matching M1 Arc controller. Set mass 10, restart game, confirm persistence, toggle master off/on, test bike inclusion.
3. No release before standalone validation. CI builds DLL, policy tests and controller syntax parse, not full runtime/semantic validation.
