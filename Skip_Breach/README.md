# Skip Breach — experimental native M1 Arc integration

**Status:** buildable source and game-root ZIP for supervised in-game testing,
**not released**. REDscript game compilation and real selector behavior have
not yet been verified in-game. The development solver and Python tests under
`dev/` are not packaged.

## What has been implemented

- `r6/scripts/M1ArcSkipBreach/M1ArcSkipBreach.reds` captures the actual
  generated matrix via `MinigameGenerationRule.OnProcessRule`, finds the
  current visible/unfulfilled daemon sequences by owner ID, and runs an
  original REDscript row-first, alternating-column/row depth-first solver.
- A bounded 50,000-node search selects the highest number of matching
  daemon sequences, breaking ties in favor of shorter paths. If the budget
  is reached, it **does not execute** the possibly suboptimal result.
- A game-system update every 0.1 seconds sends one action at a time through
  M1 Arc's `M1Arc_BreachSelectCell(wref<inkLogicController>, Vector2)`. The
  next cell is submitted **only after** the game's
  `HackingMinigameGameController.OnPositionSelected` acknowledgement.
- Generation/session changes, user input, a different minigame state,
  disabled settings, a missing controller or a 1.5-second acknowledgement
  timeout stop execution and restore the vanilla timer.
- Nothing writes daemon status, success facts, rewards or a fake result.

## Install and test

Install the **matching M1 Arc PR #1 development artifact** first (the
revision containing `M1Arc_BreachInputAvailable`), and RED4ext + REDscript.
Unpack the standalone Skip Breach GitHub Actions artifact **once** into
the Cyberpunk 2077 game root. It contains:

```text
red4ext/plugins/m1arc/mods/m1nsook.skipbreach/
├── mod.json
├── settings.json
└── language/
    ├── en_US.json
    └── en_US.json.example
r6/scripts/M1ArcSkipBreach/M1ArcSkipBreach.reds
r6/scripts/M1ArcSkipBreach/M1ArcBreachNative.reds
```

The experimental native declarations live **with Skip Breach**, not
`r6/scripts/m1arc/M1Arc.reds`. If RED4ext reports native validation
errors after a game update, removing `r6/scripts/M1ArcSkipBreach/`
(and its manifest) leaves the previously working M1 Arc core API intact.

Open M1 Arc. **Automatically solve Breach Protocol** is **OFF by default**.
When first testing, ensure no existing AutoBreach installation is present:
its REDscript controller observers can conflict, and its CET/DLL files
are not required by Skip Breach.

Enable this toggle only when you deliberately want to test it on the
supported **Cyberpunk 2077 2.5 executable 3.0.88.46235**. M1 Arc validates
the exact executable version and checks for a unique native function
entry. If validation fails, Skip Breach cannot dispatch actions, and the
regular minigame remains playable.

The English localization templates are supplied for translators. The
M1 Arc language switcher is a **separate upcoming milestone**; the schema
English labels are currently the displayed strings.

**Before release:** confirm REDscript compilation, automatic appearance
in M1 Arc, opening a real Breach Protocol board, valid cell selections,
normal rewards and game focus/cancellation. Windows CI only compiles
M1 Arc C++ and verifies Python solver and source/package contracts; it
does not compile game REDscript.

## Interface design

This feature uses the user-supplied AutoBreach 0.4.0 ZIP as an
interoperability reference for controller type names, callbacks,
`Vector2(row,column)` positions and native game version. We do **not**
redistribute its `AutoBreachInput.dll`, CET Lua implementation or solver
source. M1 Arc's version-verified selector is written independently.

The offline proof tools under `Skip_Breach/dev/` remain available to
validate puzzle path legality, input capture conventions and state
acknowledgement without sending any real clicks.
