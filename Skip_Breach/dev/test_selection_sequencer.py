"""Tests exercise a fake game UI, never OS input or an installed game."""
import unittest
from dataclasses import replace

from selection_sequencer import (
    BoardSnapshot, BreachSequencer, Step,
)


class FakeGame:
    def __init__(self):
        self.board = BoardSnapshot(
            session_id="breach-1",
            matrix=(("1C", "55"), ("7A", "BD")),
            daemons=(("1C", "7A", "BD"),),
            buffer_size=3,
        )
        self.requests: list[tuple[int, int]] = []
        self.accept = True

    def snapshot(self) -> BoardSnapshot:
        return self.board

    def select_cell(self, position: tuple[int, int]) -> bool:
        self.requests.append(position)
        return self.accept

    def acknowledge(self, position: tuple[int, int]) -> None:
        self.board = replace(
            self.board,
            selected=self.board.selected + (position,),
            selection_serial=self.board.selection_serial + 1,
        )


class SequencerTests(unittest.TestCase):
    def test_first_intro_waits_then_dispatches_on_same_board(self):
        game = FakeGame()
        game.board = replace(game.board, game_state=0, capture_serial=42)
        seq = BreachSequencer(game, ack_timeout=0.25)
        self.assertEqual(seq.begin().state, Step.IDLE)
        # Base-game timerWaitsForInteraction=True: game can stay in
        # Unknown until the first accepted selection. The FIRST board
        # must become eligible without a synthetic state transition.
        for t in (0.0, 0.58, 1.02, 1.95, 2.35, 2.99):
            result = seq.tick(t)
            self.assertEqual(result.state, Step.WAITING)
            self.assertIn("intro", result.reason)
            self.assertEqual(game.requests, [])
        result = seq.tick(3.01)
        self.assertEqual(result.attempted, (0, 0))
        self.assertEqual(game.requests, [(0, 0)])
        self.assertEqual(seq.tick(3.02).state, Step.WAITING)
        self.assertEqual(game.requests, [(0, 0)])
        # Native first cell starts vanilla InProgress, not the reverse.
        game.acknowledge((0, 0))
        game.board = replace(game.board, game_state=1)
        self.assertEqual(seq.tick(3.05).state, Step.CONFIRMED)
        self.assertEqual(len(game.requests), 1)

    def test_initial_unknown_can_continue_without_inprogress_before_first_selection(self):
        game = FakeGame()
        game.board = replace(game.board, game_state=0)
        seq = BreachSequencer(game, intro_delay=3.0)
        seq.begin()
        self.assertFalse(game.requests)
        self.assertEqual(seq.tick(0).state, Step.WAITING)
        self.assertEqual(seq.tick(3.0).attempted, (0, 0))
        self.assertEqual(game.board.game_state, 0)
        self.assertEqual(seq.tick(3.1).state, Step.WAITING)
        self.assertEqual(game.requests, [(0, 0)])
        game.acknowledge((0, 0))
        self.assertEqual(seq.tick(3.2).state, Step.CONFIRMED)

    def test_first_intro_terminal_fails_closed_without_dispatch(self):
        for terminal in (2, 3):
            game = FakeGame()
            game.board = replace(game.board, game_state=0)
            seq = BreachSequencer(game)
            seq.begin()
            self.assertEqual(seq.tick(0).state, Step.WAITING)
            game.board = replace(game.board, game_state=terminal)
            self.assertEqual(seq.tick(1.0).state, Step.ABORTED)
            self.assertEqual(game.requests, [])

    def test_no_duplicate_after_dispatched_unknown_transition(self):
        game = FakeGame()
        seq = BreachSequencer(game, ack_timeout=0.1)
        seq.begin()
        self.assertEqual(seq.tick(0).attempted, (0, 0))
        game.board = replace(game.board, game_state=0)
        self.assertEqual(seq.tick(.01).state, Step.ABORTED)
        game.board = replace(game.board, game_state=1)
        self.assertEqual(seq.tick(.20).state, Step.ABORTED)
        self.assertEqual(game.requests, [(0, 0)])

    def test_clicks_exactly_one_node_then_waits_for_ack(self):
        game = FakeGame()
        seq = BreachSequencer(game)
        self.assertEqual(seq.begin().state, Step.IDLE)
        self.assertEqual(seq.tick(0).attempted, (0, 0))
        self.assertEqual(seq.tick(0.1).state, Step.WAITING)
        self.assertEqual(len(game.requests), 1)
        game.acknowledge((0, 0))
        self.assertEqual(seq.tick(0.2).state, Step.CONFIRMED)
        self.assertEqual(len(game.requests), 1)
        self.assertEqual(seq.tick(0.3).attempted, (1, 0))
        game.acknowledge((1, 0))
        self.assertEqual(seq.tick(0.4).state, Step.CONFIRMED)
        self.assertEqual(seq.tick(0.5).attempted, (1, 1))
        game.acknowledge((1, 1))
        self.assertEqual(seq.tick(0.6).state, Step.COMPLETE)
        self.assertEqual(game.requests, [(0, 0), (1, 0), (1, 1)])
        self.assertIsNone(seq.tick(0.7).attempted)

    def test_never_clicks_if_game_not_ready(self):
        for changed in (
            {"active": False},
            {"focused": False},
            {"overlay_open": True},
            {"session_id": ""},
        ):
            game = FakeGame()
            game.board = replace(game.board, **changed)
            seq = BreachSequencer(game)
            self.assertEqual(seq.begin().state, Step.ABORTED)
            self.assertFalse(game.requests)

    def test_cancels_on_focus_loss_after_click(self):
        game = FakeGame()
        seq = BreachSequencer(game)
        seq.begin()
        seq.tick(10)
        game.board = replace(game.board, focused=False)
        self.assertEqual(seq.tick(10.2).state, Step.ABORTED)
        self.assertEqual(len(game.requests), 1)

    def test_cancels_on_overlay_open_after_click(self):
        game = FakeGame()
        seq = BreachSequencer(game)
        seq.begin()
        seq.tick(0)
        game.board = replace(game.board, overlay_open=True)
        self.assertEqual(seq.tick(.1).state, Step.ABORTED)

    def test_cancels_on_new_game_or_board_changes(self):
        for changed in (
            {"session_id": "breach-2"},
            {"capture_serial": 1},
            {"matrix": (("1C", "55"), ("55", "BD"))},
            {"buffer_size": 2},
            {"daemons": (("1C",),)},
        ):
            game = FakeGame()
            seq = BreachSequencer(game)
            seq.begin()
            game.board = replace(game.board, **changed)
            self.assertEqual(seq.tick(1).state, Step.ABORTED)
            self.assertEqual(game.requests, [])

    def test_cancels_when_manual_selection_diverges(self):
        game = FakeGame()
        seq = BreachSequencer(game)
        seq.begin()
        game.board = replace(game.board, selected=((0, 1),))
        self.assertEqual(seq.tick(1).state, Step.ABORTED)
        self.assertEqual(game.requests, [])

    def test_selection_must_have_game_event_serial(self):
        game = FakeGame()
        seq = BreachSequencer(game, ack_timeout=0.2)
        self.assertEqual(seq.begin().state, Step.IDLE)
        seq.tick(0)
        # A fabricated/synthetic selected list cannot substitute for the
        # game's OnPositionSelected callback acknowledgement.
        game.board = replace(game.board, selected=((0, 0),))
        self.assertEqual(seq.tick(.1).state, Step.WAITING)
        self.assertEqual(seq.tick(.25).state, Step.ABORTED)
        self.assertEqual(game.requests, [(0, 0)])

    def test_unsolicited_selection_event_cancels(self):
        game = FakeGame()
        seq = BreachSequencer(game)
        seq.begin()
        game.board = replace(game.board, selection_serial=1)
        self.assertEqual(seq.tick(.1).state, Step.ABORTED)
        self.assertEqual(game.requests, [])

    def test_cancels_on_mismatch_after_click(self):
        game = FakeGame()
        seq = BreachSequencer(game)
        seq.begin()
        seq.tick(0)
        game.acknowledge((0, 1))
        self.assertEqual(seq.tick(.1).state, Step.ABORTED)
        self.assertEqual(len(game.requests), 1)

    def test_timeout_does_not_issue_duplicate_clicks(self):
        game = FakeGame()
        seq = BreachSequencer(game, ack_timeout=.25)
        seq.begin()
        seq.tick(0)
        self.assertEqual(seq.tick(.24).state, Step.WAITING)
        self.assertEqual(seq.tick(.26).state, Step.ABORTED)
        self.assertEqual(game.requests, [(0, 0)])

    def test_adapter_failure_aborts(self):
        game = FakeGame()
        game.accept = False
        seq = BreachSequencer(game)
        seq.begin()
        self.assertEqual(seq.tick(0).state, Step.ABORTED)
        self.assertEqual(len(game.requests), 1)

    def test_requires_fresh_unmodified_board(self):
        game = FakeGame()
        game.board = replace(game.board, selected=((0, 0),))
        seq = BreachSequencer(game)
        self.assertEqual(seq.begin().state, Step.ABORTED)
        self.assertEqual(game.requests, [])

    def test_no_solution_means_no_click(self):
        game = FakeGame()
        game.board = replace(game.board, daemons=(("FF", "FF"),))
        seq = BreachSequencer(game)
        self.assertEqual(seq.begin().state, Step.ABORTED)
        self.assertEqual(game.requests, [])

    def test_closed_panel_before_last_ack_never_claims_success(self):
        game = FakeGame()
        seq = BreachSequencer(game)
        seq.begin()
        seq.tick(0)
        game.acknowledge((0, 0))
        seq.tick(.1)
        seq.tick(.2)
        game.acknowledge((1, 0))
        seq.tick(.3)
        seq.tick(.4)
        game.board = replace(game.board, active=False)
        self.assertEqual(seq.tick(.5).state, Step.ABORTED)


if __name__ == "__main__":
    unittest.main()
