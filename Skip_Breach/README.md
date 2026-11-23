### Historical no-op root cause: module-qualified scriptable system lookup

The user supplied RED4ext and REDscript logs from Nov 23, 2026. They
confirm both the **native selector was READY** and REDscript **compiled
without errors**. The actual REDscript code still retrieved
`M1ArcSkipBreachSystem` using an **unqualified CName** despite defining
its class inside `module MisoSkipBreach`. REDscript's ScriptableSystems
container expects the full `MisoSkipBreach.M1ArcSkipBreachSystem` name.
The old lookup can return null; all controller hooks were guarded by
`IsDefined(sys)`, so capture and automatic solving were skipped silently.

This lookup is fixed, but newer logs proved another separate bug:
the first board was captured without any successful input, whereas the
second puzzle accepted every native selection. Source and CI alone do
not establish first-puzzle gameplay success.

# Skip Breach — experimental native M1 Arc integration

**Status:** buildable source and game-root ZIP for supervised in-game testing,
**not released**. REDscript game compilation and real selector behavior have
not yet been verified in-game. The development solver and Python tests under
`dev/` are not packaged.

## November 23 09:09 first-board state gating

New game log: the first board initialized at 09:09:29.383 and the
game-native selector was called at 09:09:30.391 while state was still
`Unknown`. No `OnPositionSelected` ever acknowledged that dispatch.
At 09:09:31.403 it briefly reported `InProgress` and entered a
terminal state 26 ms later. The second board, by contrast, reached
`InProgress` **before** dispatch (09:10:35.758) and acknowledged all six
selections between 09:10:36.006 and 09:10:36.135.

The source supplied by the AutoBreach author also requires
`HackingMinigameState.InProgress` before selecting anything. Its
native DLL's input handler calls a game selection function; static
analysis does not establish a separate first-board activation step.
Thus the current build **never dispatches from Unknown**. It leaves the
first capture pending until the game reports **20 sustained UI
updates of InProgress** and the *actual planned first cell* has live
grid geometry. No 3-second delay, synthetic Blackboard mutation,
blind input retries, or cancellation/reopening workaround.

**Important limitation:** the first InProgress signal occurred immediately
before a terminal event. This may be a cancellation transition, not
normal readiness. If the game never reaches sustained InProgress on an
untouched first board, this implementation will safely leave that board
manual. That means it has **not** solved the original first-screen bug.
Do not ask the player to repeat the mission that fails on cancellation.

## November 23 08:46 game log: Win32 pointer path failed

User-provided M1 Arc log showed Skip Breach captured/planned both
the first and second puzzle, and the native Win32 helper reported
`Physical grid click sent` each time. **Neither produced an
`OnPositionSelected` acknowledgement.** A later manual grid
selection did produce the game's callback. This is why the current
revision **no longer uses the Win32 `SendInput` route**.

We restored M1 Arc's version-verified internal grid selector, which
previously acknowledged all cells on the second puzzle, while keeping
first-board capture, live-grid readiness, cached planning, session
locking, and one-dispatch/one-ack safety. This is a source-level
correction only, **not** a claim that first-board behavior is fixed.
No repeat test of the user's mission is requested.

The author's mention of **Microsoft Visual C++ Redistributable**
identifies standard native C++ runtime dependencies; it does **not**
explain the game selection ABI or demonstrate any cryptocurrency miner.

## What has been implemented

- `r6/scripts/M1ArcSkipBreach/M1ArcSkipBreach.reds` captures the actual
  generated matrix via `MinigameGenerationRule.OnProcessRule`, finds the
  current visible/unfulfilled daemon sequences by owner ID, and runs an
  original REDscript row-first, alternating-column/row depth-first solver.
- A bounded 50,000-node search selects the best matching daemon path found,
  breaking ties in favor of shorter paths. If the search budget is reached,
  it now executes a **verified legal, rewarding best-found path**, reporting
  that it may not be globally optimal instead of silently doing nothing.
- A session-tagged **UI event pump** validates the actual rendered grid
  cell positions, then calls M1 Arc's built-in game-native selector
  `M1Arc_BreachSelectCell(controller, Vector2(row,col))`.
  There is no separate DLL, OCR, CMD app or external solver. The next
  cell is submitted **only after** the game's
  `HackingMinigameGameController.OnPositionSelected` acknowledgement.
- Generation/session changes, user input, a different minigame state,
  disabled settings, a missing controller or a 1.5-second acknowledgement
  timeout stop execution; **the vanilla countdown is never paused or
  overridden by Skip Breach**.
- Nothing writes daemon status, success facts, rewards or a fake result.
- **Runtime diagnostics now appear in M1 Arc's existing log:** 
  `Cyberpunk 2077/red4ext/logs/m1arc*.log` (choose the newest dated
  file), under the prefix `[Skip Breach]`. The addon calls its own
  `M1Arc_BreachTrace(String)` native function in the paired M1 Arc DLL.
  This replaces `LogChannel(DEBUG)`, whose destination/visibility was
  not confirmed. `r6/logs/redscript_rCURRENT.log` is chiefly used for
  REDscript compilation errors, not guaranteed gameplay diagnostics.
- The unified log traces first UI initialization, passive board capture,
  state transitions, planner readiness, every attempted native cell,
  matching selection callbacks and abort/timeouts. Repeated successful
  cell dispatches are **not** de-duplicated. Only transitions or actual
  input events are logged to avoid per-frame spam. No game state is written.
- **Install the matching M1 Arc PR #1 DLL along with this addon.** Older
  M1 Arc builds do not register `M1Arc_BreachTrace`; mixing builds can
  trigger invalid native declarations at game startup. A missing selector
  does not prevent passive board capture from registering.

## First-puzzle UI event execution (experimental)

The working AutoBreach ZIP updates its execution through CET's
`onUpdate` per-frame callback. Our implementation previously used
`DelaySystem.DelayCallback` to run the solver, which is a game-time
scheduler and may not advance while the first hacking UI is waiting
for a cell selection. This build replaces that periodic scheduler
with a bounded `UISystem.QueueEvent` pump, delivered to
`HackingMinigameGameController` via an added REDscript event handler.

Every event carries the exact session and capture serial, stale
pulses cannot affect a regenerated matrix, and only one outstanding
event is allowed. Safety stops exist after 1,500 UI updates.

**Base-game first-input lifecycle, sourced from the vanilla controller:**
`HackingMinigameGameController.OnInitialize` explicitly sets
`minigameData.timerWaitsForInteraction = true`. The timer does not
start until the player's first accepted matrix selection. The minigame
can remain in `HackingMinigameState.Unknown=0` before that selection.
Our previous rule demanding `InProgress=1` before sending any native
cell introduced a circular dependency on the **first** puzzle.

The user's Nov 23 log also shows the earlier native dispatch happened
only **0.587 seconds** after the first UI initialization, failed to
produce an `OnPositionSelected` acknowledgement, and stopped. Subsequent
`InProgress` transitions happened **92 ms before terminal failure**
on the first screen: these are not reliable readiness signals. The
second board entered InProgress, then accepted all native selections.

**Updated source behavior:** require an actual sustained
`InProgress` state, without inventing a fixed intro delay or changing
game facts. Skip Breach preserves its original captured board while
`Unknown`; nothing is dispatched in that state. It resolves the planned
first cell from the live
`AllParts/Top/.../ContentContainer/GridContainer/Grid` ink widget path,
checks the wrapping width against the captured matrix, and validates
row/column positions against the visible first/right/down cells. Only
after both the state and geometry are ready does M1 Arc's internal
game-native selector receive a row and column. Its C++ bridge validates
game version, signature, window focus and overlay state.
The game's `OnPositionSelected` must acknowledge every selection;
a dispatched native action alone is **never counted as success**.
No unacknowledged native input is blindly repeated. The first
untouched board remains captured while Unknown=0 and native selection
begins only at sustained InProgress=1; C++/REDscript never changes
quest facts, puzzle state, rewards or timers.

**Validation:** native M1 Arc compilation, REDscript **syntax/parser**
checks, and offline sequencing/unit tests are separate from actual
first-puzzle gameplay. This is an experimental draft, not a proven fix.
The user has already repeated a mission-failing test many times:
**do not request another run of the same stealth gig**.

## First-attempt initialization and timer safety

In-game observation: the first stealth puzzle can reach the full 30.00
timer without any automatic input, while the second puzzle after
detection may solve. Code review found two sources of startup fragility:
(1) a `GetControllerByType` failure during UI initialization stopped
execution and marked the current capture handled; (2) the mod called
`PauseTheTimer()` **before** its first native selection.

The current change waits for a valid logic controller and rendered grid
on the SAME puzzle, keeps the captured generation pending until it can
actually run, and retries only when the native selector explicitly
returns **false** before dispatching a cell. If native dispatch returns true but game
`OnPositionSelected` acknowledgement never arrives, it **does not
repeat** that selection: repeated clicks could damage the puzzle.
The mod no longer pauses or resumes the vanilla timer, so failures
cannot strand its countdown in a mod-owned paused state.

This is a **source-level risk reduction, not a verified fix** for
first-time game controller initialization. Do not test it by exiting
a stealth gig or letting it trigger failure; verification should use a
repeatable non-mission puzzle or a restorable save.

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

## RED4ext startup blocker: invalid native definitions

If RED4ext shows `M1ArcBreachNative.reds contains invalid native definitions`,
the current addon native declarations were not matched by the **loaded**
`red4ext/plugins/m1arc/m1arc.dll`. In particular, a Skip Breach build
containing `M1Arc_BreachClickAt` requires an M1 Arc build that registers
that function. The earlier M1 Arc DLL only exposed the legacy
`M1Arc_BreachSelectCell` route. **A green CI parser check does not
guarantee that the game is loading the matching DLL.**

The diagnostic `D:\\a\\RED4ext\\RED4ext\\src\\dll\\Hooks\\ValidateScripts.cpp`
in RED4ext's popup is a compiler source path embedded into RED4ext,
**not a directory on the player's PC**. Don't try to create/find it.

Recovery without running the game: remove or rename
`r6/scripts/M1ArcSkipBreach/` temporarily. Keep your normal
`red4ext/plugins/m1arc/` settings framework intact.

To reinstall the experimental combination, extract the **latest M1 Arc
PR #1 native build** into the game root first, **overwriting the old
`m1arc.dll`**, then extract the matching **Skip Breach PR #46
game-root artifact**. Both artifacts must be from matching development
revisions. Do not install only a newer Skip Breach script over an
older DLL. This is a pre-game native ABI validation failure, **not
evidence that the solver or game clock is malfunctioning**.

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
source. The game-internal selector is now again the active Skip Breach route.
The alternative Win32 input path remains in the M1 Arc DLL for reference,
but is not called by Skip Breach after the failed 08:46 runtime test.
The external `yuri-sevatz/cyber_breach` executable and OCR are not bundled.

The offline proof tools under `Skip_Breach/dev/` remain available to
validate puzzle path legality, input capture conventions and state
acknowledgement without sending any real clicks.
