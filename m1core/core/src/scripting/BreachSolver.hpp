#pragma once

// Internal, pure-data Breach Protocol route planner. This does not read game
// memory, click cells or bypass the vanilla puzzle/reward lifecycle.
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace m1core::breach {

struct Daemon
{
    std::vector<std::string> sequence;
    int priority{1};
};

struct Puzzle
{
    std::vector<std::vector<std::string>> matrix;
    std::size_t bufferSize{0};
    std::vector<Daemon> daemons;
};

struct Cell
{
    std::size_t row{};
    std::size_t column{};
    bool operator==(const Cell&) const = default;
};

struct Solution
{
    std::vector<Cell> selections;
    std::vector<std::size_t> completedDaemons;
    int score{};
};

// Returns nullopt for invalid/incomplete captured puzzle input. A valid,
// unsolvable puzzle returns score=0 with no selections (never auto-submit it).
// Selection coordinates are zero-based, in the order to enter in the game.
[[nodiscard]] std::optional<Solution> Solve(const Puzzle& puzzle);

} // namespace m1core::breach
