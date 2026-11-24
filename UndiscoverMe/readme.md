# UndiscoverMe 1.1.0 — M1 Arc integration

Requirements: Cyberpunk 2077 2.50, RED4ext, redscript, TweakXL and M1 Arc 1.0.1+. CET is not required.

Upgrade: remove the old CET mod folder `bin/x64/plugins/cyber_engine_tweaks/mods/UndiscoverMe/` before installing. Old db.json settings are not imported. M1 Arc now persists the six switches and the fifth map tab writes directly to the same settings. Additional catalog remains OFF by default.

Original 212 locations and 175 journal IDs are embedded in REDscript; source TSVs remain under tools/catalog. Vanilla POI saved-state and journal completion checks, visible marker duplicate suppression and owned-pin cleanup remain. The existing optional Teleporter ownership API and master-disable fact mirror are retained. The old CET translation overlay is replaced by English, Polish and Korean M1 Arc settings text; catalog debug captions currently remain English.

The 1.1.0 migration was confirmed working in-game by the author. The optional Teleporter ownership interface remains in place; full addon and completion-regression coverage has not been exhaustively verified. No quest progression or fast-travel unlock changes.
