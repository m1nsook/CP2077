# Daemon

**Daemon** automatically solves Cyberpunk 2077's **Breach Protocol** minigame
by selecting valid grid cells inside the original puzzle. The game still
controls completion, animations, daemons and rewards.

The first-puzzle lifecycle fix in the existing Skip Breach REDscript source
was verified in seven cold launches, including a five-cell puzzle. This
refactor **does not change that tested REDscript or its native declarations**.

## Requirements

- Cyberpunk 2077 **2.50**
- RED4ext
- redscript
- **[M1 Arc 1.0.0](https://www.nexusmods.com/cyberpunk2077/mods/39311) or newer**

## Install and configure

Extract the installable Daemon ZIP directly into the Cyberpunk 2077 game
directory, with M1 Arc installed separately.

Open M1 Arc, select **Daemon** under **Gameplay** and enable
**Automatically solve Breach Protocol**. This option is **off by default**.
The next untouched Breach Protocol puzzle can be automatically solved.

The game-root package deliberately installs internal files under
`m1nsook.skipbreach` and `r6/scripts/M1ArcSkipBreach/`. These are
compatibility identifiers used by **M1 Arc 1.0.0's native input guard** and
the previously tested REDscript system. **Daemon is the public name.**
Do not install an earlier Skip Breach development ZIP alongside Daemon:
the same internal install paths are used.

## How it works

- Reads the real Breach Protocol matrix and requested daemon chains
- Searches legal first-row / alternating-column-and-row paths
- Uses the native M1 Arc cell selector to submit input to the game
- Advances only after `OnPositionSelected` acknowledges the previous cell
- Stops when the puzzle, controller, generation or enabled setting changes
- Does not skip the puzzle by changing quest facts or faking daemon rewards

English, Polish and Korean labels are included in the M1 Arc language format.

## Important

Some Breach Protocol failures can trigger immediate detection and fail a gig.
**Never cancel and reopen a puzzle as an automation workaround.**
The tested solution handles first-puzzle initialization without doing that.
The first-release compatibility target is Cyberpunk 2077 **2.50**; game
updates may change the internal selector or UI lifecycle.

Developer logs are written into the latest
`red4ext/logs/m1arc*.log` file under the original `[Skip Breach]`
diagnostic prefix. This is deliberately unchanged in M1 Arc 1.0.0.

### Source references

The source contains a bounded path solver, read-only matrix capture,
optional redscript CLI checks and unit tests. The M1 Arc framework
remains a separate dependency. M1 Arc is not bundled into Daemon.
