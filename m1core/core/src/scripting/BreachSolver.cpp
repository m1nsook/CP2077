#include "BreachSolver.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace m1core::breach {
namespace {

// The vanilla board is small. Bound imported puzzle size so corrupted
// game-state capture cannot stall the UI/game update thread.
constexpr std::size_t kMaxMatrixSide = 7;
constexpr std::size_t kMaxBufferSize = 8;
constexpr std::size_t kMaxDaemons = 8;

bool HasMatchingSuffix(const std::vector<std::string>& picked,
                       const std::vector<std::string>& daemon)
{
    if (daemon.size() > picked.size()) return false;
    return std::equal(daemon.rbegin(), daemon.rend(), picked.rbegin());
}

// Longest daemon prefix matching the current picked-buffer suffix.
// This supplies an optimistic upper bound; it must not underestimate.
std::size_t PrefixSuffix(const std::vector<std::string>& picked,
                         const std::vector<std::string>& daemon)
{
    const auto count = std::min(picked.size(), daemon.size());
    for (auto length = count; length > 0; --length)
    {
        if (std::equal(daemon.begin(), daemon.begin() + length,
                       picked.end() - length))
            return length;
    }
    return 0;
}

bool Valid(const Puzzle& puzzle)
{
    const auto side = puzzle.matrix.size();
    if (side < 2 || side > kMaxMatrixSide ||
        puzzle.bufferSize == 0 || puzzle.bufferSize > kMaxBufferSize ||
        puzzle.bufferSize > side * side ||
        puzzle.daemons.empty() || puzzle.daemons.size() > kMaxDaemons)
        return false;

    for (const auto& row : puzzle.matrix)
    {
        if (row.size() != side) return false;
        for (const auto& code : row)
            if (code.empty() || code.size() > 16) return false;
    }
    for (const auto& daemon : puzzle.daemons)
    {
        if (daemon.priority < 1 || daemon.priority > 1000 ||
            daemon.sequence.empty() || daemon.sequence.size() > kMaxBufferSize)
            return false;
        for (const auto& code : daemon.sequence)
            if (code.empty() || code.size() > 16) return false;
    }
    return true;
}
} // namespace

std::optional<Solution> Solve(const Puzzle& puzzle)
{
    if (!Valid(puzzle)) return std::nullopt;

    const auto side = puzzle.matrix.size();
    const auto daemonCount = puzzle.daemons.size();
    const auto allDone = (std::uint32_t{1} << daemonCount) - 1;
    Solution best;
    std::vector<Cell> path;
    std::vector<std::string> picked;
    path.reserve(puzzle.bufferSize);
    picked.reserve(puzzle.bufferSize);

    const auto visit = [&](auto&& self, std::size_t row, std::size_t column,
                           std::uint64_t used, std::uint32_t completed,
                           int score) -> void
    {
        const auto index = row * side + column;
        const std::uint64_t bit = std::uint64_t{1} << index;
        if ((used & bit) != 0) return;

        used |= bit;
        path.push_back({row, column});
        picked.push_back(puzzle.matrix[row][column]);

        // A daemon only becomes completed when its full contiguous suffix
        // is appended. Overlapping daemons are scored independently.
        for (std::size_t i = 0; i < daemonCount; ++i)
        {
            const auto daemonBit = std::uint32_t{1} << i;
            if ((completed & daemonBit) == 0 &&
                HasMatchingSuffix(picked, puzzle.daemons[i].sequence))
            {
                completed |= daemonBit;
                score += puzzle.daemons[i].priority;
            }
        }

        // Stopping early is legal and preferred over wasting buffer cells.
        if (score > best.score ||
            (score == best.score && score > 0 &&
             (best.selections.empty() || path.size() < best.selections.size())))
        {
            best.score = score;
            best.selections = path;
            best.completedDaemons.clear();
            for (std::size_t i = 0; i < daemonCount; ++i)
                if ((completed & (std::uint32_t{1} << i)) != 0)
                    best.completedDaemons.push_back(i);
        }

        if (path.size() < puzzle.bufferSize && completed != allDone)
        {
            const auto remaining = puzzle.bufferSize - path.size();
            int optimistic = score;
            for (std::size_t i = 0; i < daemonCount; ++i)
            {
                if ((completed & (std::uint32_t{1} << i)) != 0) continue;
                const auto& daemon = puzzle.daemons[i];
                const auto match = PrefixSuffix(picked, daemon.sequence);
                if (daemon.sequence.size() - match <= remaining)
                    optimistic += daemon.priority;
            }

            if (optimistic >= best.score)
            {
                // After first-row selection: column, then row, then column...
                const bool selectColumn = (path.size() % 2) == 1;
                for (std::size_t next = 0; next < side; ++next)
                {
                    if (selectColumn)
                        self(self, next, column, used, completed, score);
                    else
                        self(self, row, next, used, completed, score);
                }
            }
        }

        picked.pop_back();
        path.pop_back();
    };

    // The first selection must be anywhere in the TOP row.
    for (std::size_t column = 0; column < side; ++column)
        visit(visit, 0, column, 0, 0, 0);

    return best;
}
