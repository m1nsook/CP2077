#include "../core/src/scripting/BreachSolver.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error("Breach Solver: " #condition); } while (false)

namespace {
using namespace m1core::breach;

std::vector<std::string> Extract(const Puzzle& puzzle, const Solution& solution)
{
    std::vector<std::string> picked;
    std::uint64_t used = 0;
    REQUIRE(solution.selections.size() <= puzzle.bufferSize);
    for (std::size_t i = 0; i < solution.selections.size(); ++i)
    {
        const auto [row, column] = solution.selections[i];
        REQUIRE(row < puzzle.matrix.size() && column < puzzle.matrix.size());
        if (i == 0) REQUIRE(row == 0);
        if (i > 0)
        {
            const auto prior = solution.selections[i - 1];
            if (i % 2 == 1) REQUIRE(column == prior.column);
            else REQUIRE(row == prior.row);
        }
        const auto bit = std::uint64_t{1} << (row * puzzle.matrix.size() + column);
        REQUIRE((used & bit) == 0);
        used |= bit;
        picked.push_back(puzzle.matrix[row][column]);
    }
    return picked;
}

bool Includes(const std::vector<std::string>& picked, const std::vector<std::string>& daemon)
{
    return std::search(picked.begin(), picked.end(), daemon.begin(), daemon.end()) != picked.end();
}

void Verify(const Puzzle& puzzle, const Solution& result)
{
    const auto buffer = Extract(puzzle, result);
    int score = 0;
    std::vector<std::size_t> completed;
    for (std::size_t i = 0; i < puzzle.daemons.size(); ++i)
    {
        if (Includes(buffer, puzzle.daemons[i].sequence))
        {
            score += puzzle.daemons[i].priority;
            completed.push_back(i);
        }
    }
    REQUIRE(score == result.score);
    REQUIRE(completed == result.completedDaemons);
}

} // namespace

int main()
{
    // Three overlapping daemons: legal route must pick from first row,
    // alternate column/row and never reuse a cell.
    const Puzzle overlap{
        {
            {"E9","BD","55","1C","7A"},
            {"55","E9","1C","BD","FF"},
            {"1C","BD","E9","55","55"},
            {"7A","FF","BD","1C","55"},
            {"55","E9","7A","BD","7A"}
        }, 7,
        {{{"1C","55"},1},{{"55","BD"},2},{{"BD","E9","7A"},5}}
    };
    const auto solved = Solve(overlap);
    REQUIRE(solved.has_value());
    REQUIRE(solved->score == 8);
    REQUIRE(solved->completedDaemons.size() == 3);
    REQUIRE(solved->selections.size() == 5); // All overlap in five picks.
    Verify(overlap, *solved);

    // Prefer the high-value sequence when the buffer prevents all targets,
    // and prefer fewer picks when scores are otherwise identical.
    const Puzzle prioritized{
        {{"AA","BB"},{"CC","DD"}}, 2,
        {{{"AA","CC"},5},{{"BB","DD"},1},{{"BB","CC"},3}}
    };
    const auto best = Solve(prioritized);
    REQUIRE(best.has_value());
    REQUIRE(best->score == 5);
    REQUIRE(best->selections.size() == 2);
    Verify(prioritized, *best);

    // No possible sequence: NEVER generate arbitrary game input.
    const Puzzle impossible{{{"AA","BB"},{"CC","DD"}},2,{{{"ZZ","QQ"},2}}};
    const auto noPath = Solve(impossible);
    REQUIRE(noPath.has_value());
    REQUIRE(noPath->score == 0);
    REQUIRE(noPath->selections.empty());
    Verify(impossible, *noPath);

    // Invalid capture inputs must be rejected, not clicked.
    const Puzzle incomplete{{{"AA","BB"},{"CC"}},2,{{{"AA"},1}}};
    REQUIRE(!Solve(incomplete).has_value());
    auto invalidBuffer = overlap;
    invalidBuffer.bufferSize = 1000;
    REQUIRE(!Solve(invalidBuffer).has_value());
    auto invalidPriority = overlap;
    invalidPriority.daemons[0].priority = -2;
    REQUIRE(!Solve(invalidPriority).has_value());

    std::cout << "m1core Breach Protocol: optimal legal path, priorities, no-click failure and invalid capture passed\n";
}
