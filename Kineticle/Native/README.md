# Kineticle native diagnostic build (dev.7)

Standalone Core requires RED4ext only. Optional M1 Arc controller supplies settings, report actions and sampled player context. Install both matching archives for the comprehensive test. Do not mix with the dev.5 REDscript fallback or old native controllers. Version-gated to game executable 3.0.88.46235 / RED4ext 1.31.1.0.

**No gameplay writes.** This consolidated build replaces the single-purpose probe cycle. It measures the existing verified sync and bump dispatcher hooks, checks all available safe guards independently on both vehicles, captures per-query failure reasons, physics-state masks, masses, inverse mass, timing, velocities, angular velocities, force/torque accumulators, and dry-run recovery/impulse estimates. Nearby duplicate events are flagged rather than discarded. The estimates use observed sync delta velocity, not a proven contact impulse. The address checks for native impulse functions do not call them. The 8-event histories and 512-sample ring are bounded; the post-sync timing excludes the original physics function, pre-sync work and some early returns. No exact process-memory or allocation profiling is claimed.

The optional controller samples player presence, mounted vehicle identity, driver, PSM vehicle state and all three known restriction tags every 0.5 s while enabled. Each pair reports the sampled context, its age and whether the ID/time match. This is not exact event-time proof and not standalone native PSM access. PSM values: -1 unknown, 0 default, 1 driving, 2 combat, 3 passenger, 4 transition, 5 turret, 6 driver combat, 7 scene.

Report writes the complete snapshot to `red4ext/plugins/Kineticle/diagnostics.txt` using atomic replacement, with numbered bounded M1 Arc log pages as a secondary transport. A normal unload also writes a report when no manual capture succeeded; a crash may prevent that fallback. Reset resets counters/history; it does not erase the existing disk report. Report again before sending the file. Old logs are retained by M1 Arc; prefer the disk file. No full WolvenKit export needed.

## One test session
1. Reset while on foot. Walk/stand, enter car, drive normally, hit a wall, collide with another vehicle (including head-on), then exit. If practical, repeat with a bike and try passenger or remote control. Do not seek quest states that are unavailable.
2. Press Report once after the final test. Send diagnostics.txt; if it fails, copy numbered M1 Arc diagnostics.
3. No gameplay difference is expected. Guard results of -1 are unknown, not false; temporal matches are not causal proof.

## Remaining work
Exact contact identification at the physics layer, actual native impulse ABI/delivery, standalone event-time PSM/status tags, thread authority, environment contact coverage, and native damage/visual/tire/injury hooks remain unverified. Unsupported or unknown checks must never be treated as permission to modify gameplay. No new guessed offsets or third-party runtime dependencies.

References: pinned [SDK](https://github.com/jackhumbert/RED4ext.SDK/tree/21efd4250f18f6703d6ff7caba0245093b834859), [LTBF sync hook](https://github.com/jackhumbert/let_there_be_flight/blob/main/src/red4ext/Physics/ChassisContacts.cpp), [CET dispatcher reference](https://github.com/maximegmd/CyberEngineTweaks/blob/d7c378d4e9c7b15ca1ce42c37e8732d3b397b047/src/scripting/FunctionOverride.cpp#L153-L194), prior verified dev.5 REDscript guard code under ../Core. SDK and JSON licenses included by CI.
