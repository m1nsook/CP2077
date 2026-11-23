"""Source-derived Cyberpunk 2.5 capture contract tests."""
import unittest
from dataclasses import replace

from native_capture import Chain, Program, capture_from_game
from selection_sequencer import BreachSequencer, Step
from solver import path_is_valid, solve


class NativeCaptureTests(unittest.TestCase):
    def sample(self, **changes):
        params = dict(
            session_id="breach-1",
            capture_serial=8,
            selection_serial=0,
            grid_rows=((11, 22, 33), (44, 55, 66), (77, 88, 99)),
            buffer_size=4,
            programs=(Program(), Program(hidden=True), Program(), Program()),
            chains=(
                Chain(owner_id=0, tokens=(22, 55)),
                Chain(owner_id=1, tokens=(11, 22)),
                Chain(owner_id=2, tokens=(55, 44)),
                Chain(owner_id=3, tokens=(66, 99), fulfilled=True),
            ),
        )
        params.update(changes)
        return capture_from_game(**params)

    def test_display_order_hidden_and_fulfilled_skipped(self):
        b = self.sample()
        self.assertEqual(b.daemons, (("22", "55"), ("55", "44")))
        self.assertEqual(b.capture_serial, 8)
        self.assertEqual(b.matrix[0], ("11", "22", "33"))

    def test_native_row_col_does_not_transpose(self):
        board = self.sample(
            game_selected_row_columns=((0, 1), (1, 1)),
            selection_serial=2,
        )
        self.assertEqual(board.selected, ((0, 1), (1, 1)))
        self.assertEqual(board.matrix[1][1], "55")
        self.assertTrue(path_is_valid(board.matrix, board.selected))
        solution = solve(board.matrix, board.daemons, board.buffer_size)
        self.assertIsNotNone(solution)
        self.assertTrue(path_is_valid(board.matrix, solution.cells))

    def test_no_active_visible_program_fails_closed(self):
        with self.assertRaises(ValueError):
            self.sample(programs=(Program(hidden=True),) * 4)

    def test_mismatched_grid_and_duplicate_owners_fail_closed(self):
        with self.assertRaises(ValueError):
            self.sample(grid_rows=((11, 22), (33,)))
        with self.assertRaises(ValueError):
            self.sample(chains=(Chain(0, (11,)), Chain(0, (22,))))

    def test_invalid_engine_state_fails_closed(self):
        for state in (0, 2, 3, -1):
            with self.subTest(state=state), self.assertRaises(ValueError):
                self.sample(game_state=state)

    def test_repeated_board_with_new_generation_serial_is_not_same_puzzle(self):
        board = self.sample()
        changed = replace(board, capture_serial=board.capture_serial + 1)
        self.assertNotEqual(board.fingerprint(), changed.fingerprint())

    def test_visible_daemons_capped_to_four(self):
        b = self.sample(
            programs=(Program(),) * 5,
            chains=tuple(Chain(i, (11, 22)) for i in range(5)),
        )
        self.assertEqual(len(b.daemons), 4)


if __name__ == "__main__":
    unittest.main()
