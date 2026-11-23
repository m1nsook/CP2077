"""Unit tests for the Skip Breach *pure path solver*, not game input."""
import unittest
from solver import path_is_valid, solve


class SkipBreachSolverTests(unittest.TestCase):
    def test_optimizes_overlapping_daemons(self):
        matrix = [
            ["55", "1C", "BD"],
            ["7A", "E9", "1C"],
            ["BD", "55", "7A"],
        ]
        result = solve(matrix, [["55", "7A"], ["7A", "1C"]], 4)
        self.assertIsNotNone(result)
        self.assertEqual(result.matched_daemons, (0, 1))
        self.assertEqual(result.codes, ("55", "7A", "1C"))
        self.assertEqual(result.cells, ((0, 0), (1, 0), (1, 2)))
        self.assertTrue(path_is_valid(matrix, result.cells))
        self.assertTrue(result.exhaustive)

    def test_cyberpunk_2x_real_seven_by_seven_matrix(self):
        # Reconstructed from the user-reported game screenshot before the
        # first manual selection. Row 0 last cell was already selected:
        # restore its buffer code "1C" for this independent solver test.
        matrix = [
            ["7A", "7A", "E9", "55", "7A", "55", "1C"],
            ["1C", "7A", "FF", "55", "55", "BD", "55"],
            ["1C", "E9", "55", "1C", "1C", "FF", "1C"],
            ["1C", "7A", "E9", "7A", "BD", "7A", "1C"],
            ["7A", "1C", "7A", "7A", "55", "E9", "1C"],
            ["55", "7A", "E9", "7A", "7A", "E9", "55"],
            ["55", "BD", "7A", "55", "1C", "1C", "55"],
        ]
        daemons = [["1C", "1C"], ["7A", "1C"], ["7A", "7A", "7A"]]
        # Known route solves all three programs in five legal selections.
        path = ((0, 0), (4, 0), (4, 3), (2, 3), (2, 0))
        self.assertTrue(path_is_valid(matrix, path))
        codes = tuple(matrix[r][c] for r, c in path)
        self.assertEqual(codes, ("7A", "7A", "7A", "1C", "1C"))
        for daemon in daemons:
            self.assertTrue(any(
                codes[i:i + len(daemon)] == tuple(daemon)
                for i in range(len(codes) - len(daemon) + 1)
            ))
        result = solve(matrix, daemons, 8, node_budget=50_000)
        self.assertIsNotNone(result)
        self.assertTrue(path_is_valid(matrix, result.cells))
        self.assertEqual(set(result.matched_daemons), {0, 1, 2})
        self.assertLessEqual(len(result.cells), 8)

    def test_first_row_and_alternation(self):
        matrix = [["11", "22", "33"],
                  ["44", "55", "66"],
                  ["77", "88", "99"]]
        result = solve(matrix, [["22", "55", "11", "44"]], 5)
        # 22(0,1) -> 55(1,1) is valid; 11(0,0) cannot follow because
        # the next cell MUST be selected from row 1, not row 0.
        self.assertIsNone(result)

    def test_bridging_cells_and_duplicate_prevention(self):
        matrix = [["1C", "55"],
                  ["7A", "BD"]]
        result = solve(matrix, [["1C", "7A", "BD"]], 3)
        self.assertEqual(result.cells, ((0, 0), (1, 0), (1, 1)))
        self.assertTrue(path_is_valid(matrix, result.cells))
        self.assertEqual(len(set(result.cells)), len(result.cells))

    def test_impossible_reward_does_not_fake_success(self):
        self.assertIsNone(solve([["1C", "55"], ["BD", "7A"]],
                                [["FF", "FF"]], 4))

    def test_limits_are_explicit_and_safe(self):
        with self.assertRaises(ValueError):
            solve([["1C"]], [["1C"]], 0)
        with self.assertRaises(ValueError):
            solve([["1C"], []], [["1C"]], 2)
        with self.assertRaises(ValueError):
            solve([["1C"]], [["1C"]], 2, node_budget=0)

    def test_budget_never_mislabels_approximate_result_as_exact(self):
        matrix = [["55", "7A", "1C"],
                  ["7A", "55", "1C"],
                  ["1C", "7A", "BD"]]
        result = solve(matrix, [["55"]], 7, node_budget=1)
        self.assertIsNotNone(result)
        self.assertFalse(result.exhaustive)

    def test_validate_detects_illegal_clicks(self):
        board = [["11", "22"], ["33", "44"]]
        self.assertFalse(path_is_valid(board, [(1, 0)]))       # first row only
        self.assertFalse(path_is_valid(board, [(0, 0), (1, 1)]))
        self.assertFalse(path_is_valid(board, [(0, 0), (1, 0), (0, 0)]))
        self.assertTrue(path_is_valid(board, [(0, 0), (1, 0), (1, 1)]))


if __name__ == "__main__":
    unittest.main()
