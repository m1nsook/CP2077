"""Fail-closed Breach Protocol *selection sequencer*.

Development-only interface, not shipped in Cyberpunk 2077. An eventual
REDscript/game adapter must supply actual snapshots and select real nodes.
A fake adapter proves the sequencing logic without sending OS/game input.
"""
from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Protocol

from solver import Position, Solution, path_is_valid, solve


@dataclass(frozen=True)
class BoardSnapshot:
    """Immutable view of one real puzzle as supplied by a future game adapter."""
    session_id: str
    matrix: tuple[tuple[str, ...], ...]
    daemons: tuple[tuple[str, ...], ...]
    buffer_size: int
    selected: tuple[Position, ...] = ()
    active: bool = True
    focused: bool = True
    overlay_open: bool = False

    def fingerprint(self) -> tuple[object, ...]:
        return self.session_id, self.matrix, self.daemons, self.buffer_size


class SelectionAdapter(Protocol):
    """Target-specific bridge; never infer game state from screen pixels."""

    def snapshot(self) -> BoardSnapshot:
        ...

    def select_cell(self, position: Position) -> bool:
        """Request exactly ONE cell; returning True is not acknowledgement."""
        ...


class Step(Enum):
    IDLE = "idle"
    WAITING = "waiting"
    CONFIRMED = "confirmed"
    COMPLETE = "complete"
    ABORTED = "aborted"


@dataclass(frozen=True)
class StepResult:
    state: Step
    reason: str
    attempted: Position | None = None


class BreachSequencer:
    """Single-run coordinator for a prevalidated legal path.

    At most one click request may be outstanding; every next step requires
    observing the exact newly selected coordinate in the actual game state.
    No forced-success actions, daemon modifications or blind click loops.
    """

    def __init__(self, adapter: SelectionAdapter, *, ack_timeout: float = 1.0):
        if ack_timeout <= 0:
            raise ValueError("ack_timeout must be positive")
        self.adapter = adapter
        self.ack_timeout = ack_timeout
        self.solution: Solution | None = None
        self._fingerprint: tuple[object, ...] | None = None
        self._confirmed = 0
        self._pending_time: float | None = None
        self._ended = False

    @staticmethod
    def _safe(board: BoardSnapshot) -> bool:
        return (
            board.active
            and board.focused
            and not board.overlay_open
            and bool(board.session_id)
            and bool(board.matrix)
        )

    def begin(self, *, allow_approximate: bool = False) -> StepResult:
        """Plan once, from a fresh unmodified puzzle; do not click here."""
        if self.solution is not None or self._ended:
            return self._abort("sequencer already started or ended")
        board = self.adapter.snapshot()
        if not self._safe(board):
            return self._abort("game UI unavailable, unfocused or overlay active")
        if board.selected:
            return self._abort("puzzle already contains manual selections")
        try:
            solution = solve(board.matrix, board.daemons, board.buffer_size)
        except (TypeError, ValueError):
            return self._abort("invalid puzzle data")
        if solution is None:
            return self._abort("no rewarding legal path")
        if not solution.exhaustive and not allow_approximate:
            return self._abort("solver budget exceeded; no verified optimum")
        if not path_is_valid(board.matrix, solution.cells):
            return self._abort("solver produced an illegal path")
        self.solution = solution
        self._fingerprint = board.fingerprint()
        return StepResult(Step.IDLE, "legal path planned")

    def tick(self, now: float) -> StepResult:
        """Advance no more than one action or acknowledgement per call.

        The injected clock is monotonic; callers must never run this from a
        render callback with synthetic or variable time steps.
        """
        if self._ended:
            return StepResult(Step.ABORTED, "sequencer stopped")
        if self.solution is None:
            return self._abort("no planned path")
        board = self.adapter.snapshot()
        if not self._safe(board):
            return self._abort("game UI unavailable, unfocused or overlay active")
        if board.fingerprint() != self._fingerprint:
            return self._abort("puzzle changed or new session started")
        cells = self.solution.cells

        if self._pending_time is not None:
            desired = cells[:self._confirmed + 1]
            if board.selected == desired:
                self._pending_time = None
                self._confirmed += 1
                if self._confirmed == len(cells):
                    self._ended = True
                    return StepResult(Step.COMPLETE, "all cells acknowledged")
                return StepResult(Step.CONFIRMED, "selected node acknowledged")
            if board.selected != cells[:self._confirmed]:
                return self._abort("game selection diverged from planned path")
            if now - self._pending_time > self.ack_timeout:
                return self._abort("game did not acknowledge selection")
            return StepResult(Step.WAITING, "waiting for game acknowledgement")

        if board.selected != cells[:self._confirmed]:
            return self._abort("manual selection or out-of-order state")
        if self._confirmed == len(cells):
            self._ended = True
            return StepResult(Step.COMPLETE, "all cells acknowledged")
        position = cells[self._confirmed]
        # The adapter is expected to check game focus/UI again immediately
        # before the physical click or in-game widget event.
        if not self.adapter.select_cell(position):
            return self._abort("selection adapter rejected click")
        self._pending_time = now
        return StepResult(Step.WAITING, "selection requested", position)

    def _abort(self, reason: str) -> StepResult:
        self._ended = True
        self._pending_time = None
        return StepResult(Step.ABORTED, reason)
