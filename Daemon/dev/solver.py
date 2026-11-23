"""Pure Breach Protocol matrix-path solver for the planned Skip Breach mod.

Development/reference code only. NOT installed in Cyberpunk 2077.
It consumes already-decoded matrix tokens and daemon sequences, then returns
the actual row/column-alternating cell positions for a valid click sequence.
It cannot access the game's UI or submit clicks by itself.

No forced success, filled daemon flags, OCR, CET, or M1 Arc script runtime.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Sequence

Position = tuple[int, int]


@dataclass(frozen=True)
class Solution:
    cells: tuple[Position, ...]
    codes: tuple[str, ...]
    matched_daemons: tuple[int, ...]
    exhaustive: bool


def _matched(codes: tuple[str, ...],
             programs: tuple[tuple[str, ...], ...]) -> tuple[int, ...]:
    matched: list[int] = []
    for idx, sequence in enumerate(programs):
        n = len(sequence)
        if n <= len(codes) and any(
            codes[start:start + n] == sequence
            for start in range(len(codes) - n + 1)
        ):
            matched.append(idx)
    return tuple(matched)


def solve(
    matrix: Sequence[Sequence[str]],
    daemons: Sequence[Sequence[str]],
    buffer_size: int,
    *,
    node_budget: int = 250_000,
) -> Solution | None:
    """Search the vanilla row-first, alternating-column/row puzzle rules.

    The first selection must be anywhere on row 0. Thereafter selections
    alternate between a cell in the previously selected COLUMN, then a
    cell in the previously selected ROW. A coordinate can be used only once.

    Reward score is number of completed daemon subsequences (including
    overlapping programs). Among equal scores, shorter paths are preferred.
    If the budget is reached, exhaustive=False clearly marks the result as
    best-effort, NOT a certified optimal path. None means no rewarding path
    was discovered (or the supplied inputs are empty).
    """
    if not isinstance(buffer_size, int) or not 1 <= buffer_size <= 12:
        raise ValueError("buffer size must be 1..12")
    if not isinstance(node_budget, int) or node_budget < 1:
        raise ValueError("node budget must be positive")
    if not matrix or not matrix[0]:
        raise ValueError("matrix cannot be empty")
    rows = len(matrix)
    cols = len(matrix[0])
    if rows > 8 or cols > 8:
        raise ValueError("maximum supported matrix size is 8x8")
    if any(len(row) != cols for row in matrix):
        raise ValueError("matrix must be rectangular")

    board = tuple(tuple(str(cell).upper() for cell in row) for row in matrix)
    programs = tuple(tuple(str(token).upper() for token in seq)
                     for seq in daemons if seq)
    if not programs:
        return None
    if any(not token for row in board for token in row):
        raise ValueError("matrix has empty cell values")
    if any(not token for seq in programs for token in seq):
        raise ValueError("daemon sequence has empty tokens")

    cap = min(buffer_size, rows * cols)
    best: Solution | None = None
    best_score = (0, 0)
    states = 0
    exhausted = False

    def visit(
        row: int, col: int, used: int,
        cells: tuple[Position, ...],
        codes: tuple[str, ...],
        pick_column: bool,
    ) -> None:
        nonlocal best, best_score, states, exhausted
        if states >= node_budget:
            exhausted = True
            return
        states += 1

        matched = _matched(codes, programs)
        score = (len(matched), -len(cells))
        if matched and score > best_score:
            best = Solution(cells, codes, matched, True)
            best_score = score

        if len(cells) >= cap:
            return
        # Rows and columns alternate after each selection, including bridge
        # cells that do not themselves belong to any requested daemon.
        if pick_column:
            possibilities = ((r, col) for r in range(rows))
        else:
            possibilities = ((row, c) for c in range(cols))
        for r, c in possibilities:
            bit = 1 << (r * cols + c)
            if used & bit:
                continue
            visit(r, c, used | bit, cells + ((r, c),),
                  codes + (board[r][c],), not pick_column)
            if exhausted:
                return

    for col in range(cols):
        visit(0, col, 1 << col, ((0, col),), (board[0][col],), True)
        if exhausted:
            break

    if best is None:
        return None
    if exhausted:
        return Solution(best.cells, best.codes, best.matched_daemons, False)
    return best


def path_is_valid(matrix: Sequence[Sequence[str]],
                  cells: Sequence[Position]) -> bool:
    """Independently check a proposed click path against vanilla rules."""
    if not matrix or not matrix[0] or not cells or cells[0][0] != 0:
        return False
    rows = len(matrix)
    cols = len(matrix[0])
    if any(len(row) != cols for row in matrix):
        return False
    seen: set[Position] = set()
    for i, (r, c) in enumerate(cells):
        if not 0 <= r < rows or not 0 <= c < cols or (r, c) in seen:
            return False
        if i:
            pr, pc = cells[i - 1]
            if i % 2 == 1 and c != pc:
                return False
            if i % 2 == 0 and r != pr:
                return False
        seen.add((r, c))
    return True
