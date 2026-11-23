"""Normalize *native* Breach Protocol capture into row-major solver state.

Derived from interface observations in the supplied AutoBreach 0.4.0
Cyberpunk 2077 2.5 installation, not copied game/controller code.
This is Python development/test logic only; no CET or DLL dependency.

Observed UI contracts:
- MinigameGenerationRule.OnProcessRule receives grid[row][column].
- GridCell.rarityValue is the code token (an integer enum/value).
- UnlockableProgram is display ordered; CharactersChain.ownerId links it.
- Hidden/already-fulfilled programs and chains are not active rewards.
- OnPositionSelected(Vector2) reports game (row, column), *not* (x, y).
- Board generation and selection each have independent serials.
"""
from __future__ import annotations

from dataclasses import dataclass

from selection_sequencer import BoardSnapshot, Position


@dataclass(frozen=True)
class Chain:
    owner_id: int
    tokens: tuple[int, ...]
    fulfilled: bool = False


@dataclass(frozen=True)
class Program:
    hidden: bool = False
    fulfilled: bool = False


def capture_from_game(
    *,
    session_id: str,
    capture_serial: int,
    selection_serial: int,
    grid_rows: tuple[tuple[int, ...], ...],
    programs: tuple[Program, ...],
    chains: tuple[Chain, ...],
    buffer_size: int,
    game_selected_row_columns: tuple[tuple[int, int], ...] = (),
    game_state: int = 1,
    focused: bool = True,
    overlay_open: bool = False,
) -> BoardSnapshot:
    """Validate immutable data read *at the real game controller*, fail closed.

    A native data adapter must call this equivalent **after** board generation
    and should record game events, not read localized UI text. No screenshot
    or arbitrary cursor coordinates are ever required.
    """
    if not session_id or capture_serial < 0 or selection_serial < 0:
        raise ValueError("session/capture/selection identity must be valid")
    if game_state != 1:
        raise ValueError("breach minigame not in interactive state")
    n = len(grid_rows)
    if not 1 <= n <= 8 or any(len(row) != n for row in grid_rows):
        raise ValueError("native grid must be a supported square matrix")
    if not 1 <= buffer_size <= 12:
        raise ValueError("invalid buffer")
    if not isinstance(programs, tuple) or not isinstance(chains, tuple):
        raise ValueError("native program data must be immutable tuples")
    for row in grid_rows:
        for token in row:
            if not isinstance(token, int) or token < 0:
                raise ValueError("invalid native grid token")

    by_owner: dict[int, Chain] = {}
    for chain in chains:
        if chain.owner_id < 0 or chain.owner_id in by_owner:
            raise ValueError("invalid or duplicated daemon owner")
        by_owner[chain.owner_id] = chain

    visible: list[tuple[str, ...]] = []
    for owner, program in enumerate(programs):
        if program.hidden or program.fulfilled:
            continue
        chain = by_owner.get(owner)
        if chain is None or chain.fulfilled or not chain.tokens:
            continue
        if any(not isinstance(token, int) or token < 0 for token in chain.tokens):
            raise ValueError("invalid daemon code")
        visible.append(tuple(str(token) for token in chain.tokens))
        if len(visible) == 4:
            break
    if not visible:
        raise ValueError("no active visible daemon sequences")
    selected: tuple[Position, ...] = tuple(game_selected_row_columns)
    if len(set(selected)) != len(selected) or any(
        not 0 <= row < n or not 0 <= col < n
        for row, col in selected
    ):
        raise ValueError("game selection history is invalid")
    if len(selected) > buffer_size:
        raise ValueError("selection exceeds buffer")
    # Maintain row-major throughout. The supplied reference's Lua solver
    # intentionally uses a transposed grid[column][row]; our solver DOES NOT.
    return BoardSnapshot(
        session_id=session_id,
        capture_serial=capture_serial,
        selection_serial=selection_serial,
        matrix=tuple(tuple(str(token) for token in row)
                     for row in grid_rows),
        daemons=tuple(visible),
        buffer_size=buffer_size,
        selected=selected,
        active=True,
        focused=focused,
        overlay_open=overlay_open,
    )
