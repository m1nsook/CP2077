#include "../core/src/scripting/SandboxCleanup.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#define REQUIRE(x) do { if (!(x)) throw std::runtime_error("sandbox cleanup test: " #x); } while (false)

int main()
{
    std::vector<std::size_t> visited;
    std::vector<std::pair<std::size_t, std::string>> reported;
    std::array<bool, 6> closed{}; // 0 is the console sandbox, never touched.

    m1core::scripts::ForEachSandboxSafely(closed.size(),
        [&](std::size_t id) {
            visited.push_back(id);
            if (id == 2) throw std::runtime_error("db:close() failed");
            if (id == 4) throw 17;
            closed.at(id) = true;
        },
        [&](std::size_t id, const char* reason) {
            reported.emplace_back(id, reason);
        });

    REQUIRE(visited == std::vector<std::size_t>({1, 2, 3, 4, 5}));
    REQUIRE(!closed[0]);
    REQUIRE(closed[1] && !closed[2] && closed[3] && !closed[4] && closed[5]);
    REQUIRE(reported.size() == 2);
    REQUIRE(reported[0].first == 2);
    REQUIRE(reported[0].second == "db:close() failed");
    REQUIRE(reported[1].first == 4);
    REQUIRE(reported[1].second == "Unknown native cleanup exception");

    // The diagnostic sink can also fail. Cleanup MUST still visit the next
    // sandbox, even when reporting a previous failure throws.
    visited.clear();
    int failures = 0;
    m1core::scripts::ForEachSandboxSafely(4,
        [&](std::size_t id) {
            visited.push_back(id);
            if (id == 1 || id == 2) throw std::runtime_error("broken logger path");
        },
        [&](std::size_t, const char*) {
            ++failures;
            throw std::runtime_error("log sink unavailable");
        });
    REQUIRE(visited == std::vector<std::size_t>({1, 2, 3}));
    REQUIRE(failures == 2);

    // A fresh state with just the console sandbox has nothing to clean.
    int calls = 0;
    m1core::scripts::ForEachSandboxSafely(1,
        [&](std::size_t) { ++calls; },
        [&](std::size_t, const char*) { ++calls; });
    REQUIRE(calls == 0);

    std::cout << "m1core M7: independent sandbox DB cleanup after failure passed\n";
}
