# Skip Breach — planned Cyberpunk 2077 mod

**Status:** development groundwork only; NOT an installable mod yet.
This directory currently has an independently tested **puzzle path solver**,
not a live REDscript UI adapter. The Python code is **test tooling only**:
it is never copied into the game and is not a runtime dependency.

## Required gameplay behavior

1. V opens the original **Breach Protocol** minigame.
2. The mod reads the game's **actual** code matrix, buffer length and daemon
   sequences. It must not invent cell positions or infer them from a screenshot.
3. Find a legal optimal/best-scoring path respecting vanilla gameplay rules:
   start on row 0, alternate selecting from the prior column and prior row,
   never select one cell twice, support overlapping daemon sequences.
4. Submit each selected cell through the **game's UI/action path**, with
   safe timing and explicit confirmation that the correct minigame remains
   active. The game displays normal animations and awards normal rewards.
5. Cancel safely if the board changes, the game is unfocused, a cutscene
   begins, the M1 Arc overlay opens, or a selection fails. Do not inject
   stray input into gameplay.

**Do not:** pre-set every daemon to fulfilled, trigger a generic forced
success action, bypass the actual grid, require CET, depend on screenshots
or OCR, or invent a new M1 Arc scripting runtime.

## Existing reusable native input component

M1 Arc [internal mouse-click helper](https://github.com/m1nsook/m1arc/blob/feat/m1arc-native-redscript-imgui/docs/INPUT_CLICK_HELPER.md)
comes from the working m1core backup and is guarded against game-window
and focus errors. It is **not yet connected to REDscript or a breach
controller** and is *not* guaranteed to work with the game's UI.
Exposing an arbitrary raw-click function is **not** the plan: the mod
should interact only with a verified active minigame.

## Selection sequencer (development-only)

`dev/selection_sequencer.py` now implements the **event-driven click
coordination contract** independent of the actual game adapter. A native
game callback must provide a fresh immutable `BoardSnapshot` and an
adapter that selects **one real cell**. The coordinator:

- takes a snapshot once at start and locks session ID, matrix,
  daemon patterns and buffer length
- rejects existing manual selections, missing game focus or open M1 Arc overlay
- sends **only one** cell selection at a time
- waits for the game's selected-cells state to acknowledge the previous cell
  before sending the next one
- cancels if the board changes, the player intervenes, focus is lost,
  selection diverges or the game does not respond within the timeout
- never reports success just because the game UI vanished before a final
  acknowledgement

`test_selection_sequencer.py` uses a **fake game adapter** and no OS
injection. This is logic-level verification, not an installable mod.

## What the supplied AutoBreach 0.4.0 ZIP confirms

This analysis is based directly on the user's supplied
`AutoBreach 35512 ...zip`, not an assumed generic minigame API:

- The ZIP installs **CET Lua** (`init.lua`, `solver.lua`, coordinate
  adapter), **REDscript** (`autobreach.reds`) and a separate
  **RED4ext binary** (`AutoBreachInput.dll`). We will **not** make CET
  or that third-party binary a Skip Breach requirement.
- The REDscript code observes `HackingMinigameGameController` lifecycle,
  blackboard minigame state and `OnPositionSelected(Vector2)`. It
  injects a `MinigameGenerationRule` to capture
  `GridCell.rarityValue` directly during board generation.
- Visible/unfulfilled `UnlockableProgram` entries are matched by
  display index to `CharactersChain.ownerId`, to obtain actual daemon
  token sequences (up to four). This avoids using localized display text.
- Native game positions are **(row, column)**. The AutoBreach Lua solver
  uses an unusual transposed **(column, row)** representation and converts
  both ways. The CP2077 Python solver is already row-major, so the
  `dev/native_capture.py` adapter retains native coordinates unchanged.
- The ZIP does **not** implement raw mouse movement as the primary
  selector. REDscript asks a native method on
  `gameuiHackingMinigameLogicController` to select a cell. The DLL
  explicitly requires Cyberpunk 2.5 build `3.0.88.46235` and verifies
  the target function signature before enabling this action. Reusing the
  previous generic mouse-click helper as the primary input would be a
  regression.
- Its runtime uses generation/selection serials, cancellation and
  acknowledgement before advancing; it pauses/resumes the minigame timer
  around execution.

**Actual game functions learned from the reference (not yet integrated):**
`MinigameGenerationRule.OnProcessRule`,
`HackingMinigameGameController.OnInitialize/OnUninitialize`,
`OnPositionSelected`, `OnGameStateChanged`,
`GetProgramsChains`, `GetUnlockablePrograms`,
`GetControllerByType` and a game-version-checked direct input call.

The new development-only `native_capture.py` and tests validate the
capture schema, display-order matching and row/column conventions; the
selection sequencer now checks a **selection-event serial** as well as
the selected path, and treats a regenerated but identical board as a
new puzzle. None of this claims a native input selector is implemented.

## Remaining steps

- Confirm the Cyberpunk 2077 2.5 \`HackingMinigameGameController\` and related
  game scripts (REDscript / WolvenKit), and verify the actual member types
  for the matrix, daemon programs, buffer and selectable cell controls.
- Port/test the solver against real controller data; do not silently
  treat the Python prototype as a shipped in-game implementation.
- Implement our **own version-verified native RED4ext cell selector**
  (not a hard-coded copy of the supplied DLL). It must fail closed when
  signatures/game versions differ, and never send arbitrary mouse input.
- Build a safe in-game selection adapter, validate node-by-node state
  changes and ensure the existing M1 Arc overlay does not own the cursor.
- Add the mod's own \`mod.json\`, \`settings.json\` and \`language/en_US.json\`
  only when those controls have **real, supported behavior**. Prefer a
  single functional enable/disable setting; no decorative switches.
- Check a complete breach in-game and verify normal rewards/animations.
  Keep the PR draft until gameplay verification.

The existing **UndiscoverMe Teleporter** is an addon with no settings
needed. **No Tracking Please** is discontinued and is not a target for
M1 Arc integration.
