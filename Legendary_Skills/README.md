# Legendary Skills — Netrunner UI probe 0.1.0-dev.1

**Status:** read-only, not a playable Prestige implementation.

This standalone REDscript prototype checks whether the existing Cyberpunk 2077 2.50 Skills screen can be modified for one skill (Netrunner) without replacing the screen.

## What's in this test

- The normal Netrunner row remains unchanged until its native skill level reaches **60**.
- At level 60 the Netrunner name becomes `NETRUNNER // PRESTIGE` and its existing XP bar turns cyan.
- Hold on the Netrunner skill row / progress bar. If the native hold event reaches 100%, the name changes to `NETRUNNER // HOLD OK`.
- If the hold does not register, report that. It means we need to adjust hit testing or use a dedicated button widget.
- The wrapper restores the original tints if the engine recycles the same widget for another skill.

**No gameplay changes whatsoever:** no level reset, XP handling, stat effects, perks, quest facts, persistent status, or reward stacking. The hold event cannot trigger a Prestige. Do not interpret `HOLD OK` as a real Prestige.

## Installation

Unzip into the Cyberpunk 2077 game root (merging `r6/`). Requires redscript and a game version matching the 2.50 script API. Codeware is *not required by this limited UI diagnostic*, though we plan to use it for the full mod.

## Test

1. Install the zip, launch the game, and load a save with **Netrunner skill at level 60**. (Do not confuse Netrunner skill with character level.)
2. Open Character > Skills. Check Netrunner label and cyan progress bar.
3. Hold on the Netrunner skill row; report whether `HOLD OK` appears. The precise action may need a dedicated input/button in the next iteration.
4. Switch between the five skills and reopen the menu; verify other skills remain vanilla.
5. If game compilation fails, send `r6/logs/redscript_rCURRENT.log` (or the newest redscript log).

**Uninstall:** Remove only `r6/scripts/LegendarySkills/LegendarySkillsPreview.reds`, then launch the game again. Your save is not modified by this probe.

The next milestone is separately testing experience capture at a capped skill and safe restoration/stacking of the original passive rewards; neither is included here.
