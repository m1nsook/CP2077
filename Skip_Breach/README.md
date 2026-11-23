### In-game no-op root cause: module-qualified scriptable system lookup

The user supplied RED4ext and REDscript logs from Nov 23, 2026. They
confirm both the **native selector was READY** and REDscript **compiled
without errors**. The actual REDscript code still retrieved
`M1ArcSkipBreachSystem` using an **unqualified CName** despite defining
its class inside `module MisoSkipBreach`. REDscript's ScriptableSystems
container expects the full `MisoSkipBreach.M1ArcSkipBreachSystem` name.
The old lookup can return null; all controller hooks were guarded by
`IsDefined(sys)`, so capture and automatic solving were skipped silently.

This patch fixes that lookup and adds a contract test for fully qualified
module-name lookups. **The game still needs one end-to-end test** of the
actual native cell selection path; startup and script compilation alone
do not establish gameplay success.

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
- A bounded 50,000-node search selects the best matching daemon path found,
  breaking ties in favor of shorter paths. If the search budget is reached,
  it now executes a **verified legal, rewarding best-found path**, reporting
  that it may not be globally optimal instead of silently doing nothing.
- A game-system update every 0.1 seconds sends one action at a time through
  M1 Arc's `M1Arc_BreachSelectCell(wref<inkLogicController>, Vector2)`. The
  next cell is submitted **only after** the game's
  `HackingMinigameGameController.OnPositionSelected` acknowledgement.
- Generation/session changes, user input, a different minigame state,
  disabled settings, a missing controller or a 1.5-second acknowledgement
  timeout stop execution and restore the vanilla timer.
- Nothing writes daemon status, success facts, rewards or a fake result.
- Since Cyberpunk 2077 2.01+, the game-native `LogChannel` function has
  no public REDscript declaration. The development-only
  `M1ArcBreachNative.reds` now includes the documented declaration to
  prevent `UNRESOLVED_FN`. This does not require an M1 Arc DLL change.
  **Before a public release:** consolidate global logging declarations
  to prevent clashes with any other installed logging-header mod.
- The mod logs state transitions under `[Skip Breach]` via REDscript's
  `LogChannel(n"DEBUG", ...)`: controller initialization, capture,
  native selector compatibility, solving, each aborted selection and completion.
  M1 Arc logs the **reason** its native selector is disabled at startup
  through RED4ext's logger (wrong game version, missing signature, etc.).
  A missing native selector no longer prevents the board capture hook from
  registering, so the failure is visible.

## First-attempt safety — gig failure is not reversible

A failed Breach Protocol can immediately trigger detection and end a gig.
**Never exit and reopen a live puzzle as an activation workaround.**
That behavior previously occurred because capture was registered only when
the setting was already ON at `OnInitialize`, and disabled captures could
be marked "handled" before later enabling. The controller now always
registers a *passive* board capture observer (which returns the original
grid unchanged). Execution can be enabled while the same board is still
untouched and interactive. If a real matrix selection was made by the
player, automation is cancelled for that board.

The previous generic `OnHandleInput(n"click")` cancellation hook could
react to a non-matrix UI click during opening and was removed. Only a
confirmed `OnPositionSelected` event counts as a manual cell selection.

**This is a source-level lifecycle correction, NOT an in-game working
claim. Do not rely on this experimental mod during an irreversible gig.**
Only advance release testing in a repeatable, non-mission Breach Protocol
or with a recoverable save, after real REDscript validation.

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

## Official REDscript CLI validation (GitHub Actions)

The `windows-redscript-cli` job downloads the official
[`jac3km4/redscript` v1.0.1 CLI](https://github.com/jac3km4/redscript/releases/tag/v1.0.1)
and verifies its SHA-256. It **always checks REDscript source parsing**
for both Skip Breach scripts and the pinned matching M1 Arc core API.
No Electron, CET, or separate scripting language is introduced.

Actual type checking/semantic validation and game script compilation
require a **private, vanilla Cyberpunk 2077 2.5**
`r6/cache/final.redscripts` bundle. These CDPR game assets cannot be
stored in our public repository. Configure GitHub Actions secrets
`REDSCRIPT_GAME_BUNDLE_URL` (private HTTPS URL to the unmodified bundle)
and `REDSCRIPT_GAME_BUNDLE_SHA256` to enable **both**
`redscript-cli lint` and `redscript-cli compile` with
`--runtime-compatible`. Temporary compiled outputs are deleted and
never uploaded.

**Without that game bundle, only parsing is actually checked; the
workflow explicitly marks semantic lint/compile as SKIPPED.** This is
not a substitute for an actual Cyberpunk game runtime verification.

## Cyberpunk 2.x-compatible solver reference

The [Breach Protocol Autosolver by Marcin Cichocki](https://github.com/marcincichocki/breach-protocol-autosolver)
is a desktop app, not a RED4ext mod. It uses screenshots and OCR,
prioritized daemon combinations, BFS/DFS, and configurable keyboard/mouse
navigation on Cyberpunk 2077 2.x. Its cross-version input does not
validate our current version-gated selector. See
[2.x solver/input design notes](docs/2X_SOLVER_REFERENCE.md).

## Interface design

This feature uses the user-supplied AutoBreach 0.4.0 ZIP as an
interoperability reference for controller type names, callbacks,
`Vector2(row,column)` positions and native game version. We do **not**
redistribute its `AutoBreachInput.dll`, CET Lua implementation or solver
source. M1 Arc's version-verified selector is written independently.

The offline proof tools under `Skip_Breach/dev/` remain available to
validate puzzle path legality, input capture conventions and state
acknowledgement without sending any real clicks.
