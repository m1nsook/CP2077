# Skip Breach — M1 Arc-native Breach Protocol autosolver

**Current status (23 November 2026):** the uploaded
`M1ArcSkipBreach.reds` fixes the first-puzzle cold-launch failure.
The user confirmed **seven successful cold launches**, including a
five-cell first puzzle with all five `OnPositionSelected` callbacks
acknowledged. The native M1 Arc C++ DLL and `M1ArcBreachNative.reds`
did **not** need modification. This is an opt-in development build,
not a general compatibility guarantee or public release.

## Cold-launch first-puzzle fix (confirmed in-game)

The original native input failed on the first board because three
script-side lifecycle bugs hid each other:

1. **Intro animation:** on initial launch the grid was already drawn
   while state was still `Unknown=0`. Previously `TryStart()` could
   dispatch before the game accepted input. Now it waits for genuine
   `InProgress=1`, stabilized for 20 UI updates. If a game build never
   transitions, a bounded 600-update Unknown fallback remains.
2. **Pulse pump termination:** a terminal Blackboard state stopped
   scheduling UI events, preventing the script from re-reading live
   state. The pump now continues through a bounded 300-update
   terminal window rather than going silent.
3. **Spurious early success:** the first game after launch reports
   `Succeeded=2` while the hacking UI still accepts input and before
   the solution is complete. `RecordGameState` ignores that premature
   value while no cells have been selected or a planned path is still
   active and incomplete. `Failed=3` is **never** ignored.

If a first-cell dispatch has no acknowledgement after 120 UI updates,
the script may retry it at most three times, **only when the
selection serial is unchanged**. Subsequent cells are never retried
without acknowledgement. It does not change game facts, mark daemon
rewards complete, pause the vanilla timer, or exit/reopen the puzzle.

The diagnostic log deliberately retains the raw Blackboard transition
value, including `[diag] state A -> B, live blackboard=C` and
`[diag] ignoring Succeeded(2)`. The cause of the premature `2`
is unknown, so these checks and diagnostics remain for future game
versions. The 600- and 300-update limits are frame/update counts,
not guaranteed wall-clock durations.

**User-supplied cold-launch verification:**

```text
09:38:08.816  state 0 -> 1 (InProgress)
09:38:08.909  [diag] ignoring Succeeded(2) ... puzzle still live
09:38:09.023  Valid path ready
09:38:09.361  five native cell dispatches, each acknowledged
09:38:09.472  All planned cells acknowledged; game applies rewards
```

## History (superseded)

Earlier approaches included a premature `Unknown` selector call,
an over-strict terminal-state abort, and Win32 `SendInput` clicks that
produced **no** `OnPositionSelected` callbacks on either board.
The working fix is entirely inside REDscript. The original
`M1Arc_BreachSelectCell` native selector continues to provide the
game input; no separate closed-source DLL, CET, or CMD tool is used.

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
- A session or generation change, confirmed manual input, disabled
  settings, missing controller or `Failed=3` stop execution.
  First-cell timeout may trigger up to three guarded retries if the
  acknowledgement serial has **not** changed; later selections are
  never resent without acknowledgement. **The vanilla countdown is
  never paused or overridden by Skip Breach**.
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

## Game UI event pump and first-attempt safety

The addon captures each puzzle through a passive
`MinigameGenerationRule.OnProcessRule` observer, then uses a
session/capture-tagged `UISystem.QueueEvent` pump rather than a
game-time `DelayCallback`, which can stall during first-interaction
startup. There is never more than one outstanding cell selection.
The current planned first cell must have valid live grid geometry
before the capture is marked handled.

The script waits for `InProgress` during the opening animation
rather than confusing **drawn cells** with **interactive cells**.
The early `Succeeded=2` exception is limited to no-selection or
incomplete active-path cases; a real `Failed=3`, regenerated
session, manual first cell, lost controller, focus change, or disabled
setting still prevents further automatic input.

There is a bounded, serial-guarded retry for the **first cell only**.
The game still owns puzzle completion and rewards. No synthetic
Blackboard state transition or quest-fact edit is performed.

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

The current uploaded script has been verified in seven successful
cold launches. For future versions, verify any additional changes using
a recoverable save; a failed stealth Breach can permanently fail a gig.

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

**Before a public release:** preserve the seven cold-launch successes
while checking additional puzzle types and game settings against
recoverable saves. Windows CI parses REDscript with the upstream CLI,
runs solver/lifecycle checks and builds the package; optional semantic
compilation requires a private game-script bundle.

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
