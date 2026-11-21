#include "../core/src/scripting/ModDispatch.hpp"

#include <map>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#define REQUIRE(x) do { if (!(x)) throw std::runtime_error("mod dispatch regression: " #x); } while (false)

struct SimulatedMod
{
    bool throwNative{};
    bool throwUnknown{};
    int init{};
    int update{};
    int draw{};
    int overlay{};
};

int main()
{
    std::map<std::string, SimulatedMod> mods{
        {"a_native_failure", {.throwNative = true}},
        {"b_unknown_failure", {.throwUnknown = true}},
        {"c_healthy", {}}
    };

    std::vector<std::string> reports;
    std::vector<std::string> order;

    const auto report = [&](std::string_view mod, std::string_view phase, const char* why)
    {
        reports.emplace_back(std::string(mod) + "/" + std::string(phase) + ": " + why);
    };

    auto dispatch = [&](std::string_view phase, auto&& callback)
    {
        m1core::scripts::ForEachModSafely(mods, phase,
            [&](const SimulatedMod& mod)
            {
                // The real ScriptStore invokes context methods here.
                if (mod.throwNative) throw std::runtime_error("broken mod callback");
                if (mod.throwUnknown) throw 17;
                callback(mod);
                order.emplace_back(std::string(phase));
            }, report);
    };

    dispatch("onInit", [](const SimulatedMod& mod) { const_cast<SimulatedMod&>(mod).init++; });
    dispatch("onUpdate", [](const SimulatedMod& mod) { const_cast<SimulatedMod&>(mod).update++; });
    dispatch("onDraw", [](const SimulatedMod& mod) { const_cast<SimulatedMod&>(mod).draw++; });
    dispatch("onOverlayOpen", [](const SimulatedMod& mod) { const_cast<SimulatedMod&>(mod).overlay++; });

    REQUIRE(mods["c_healthy"].init == 1);
    REQUIRE(mods["c_healthy"].update == 1);
    REQUIRE(mods["c_healthy"].draw == 1);
    REQUIRE(mods["c_healthy"].overlay == 1);
    REQUIRE(order == std::vector<std::string>({"onInit","onUpdate","onDraw","onOverlayOpen"}));
    REQUIRE(reports.size() == 8);
    REQUIRE(reports[0].find("a_native_failure/onInit: broken mod callback") == 0);
    REQUIRE(reports[1].find("b_unknown_failure/onInit: Unknown native C++ exception") == 0);
    REQUIRE(reports[7].find("b_unknown_failure/onOverlayOpen") == 0);

    // A broken diagnostics sink cannot prevent later mods from getting an
    // update. This is essential for runtime resilience during log failures.
    int reached = 0;
    m1core::scripts::ForEachModSafely(mods, "onUpdate",
        [&](const SimulatedMod& mod) {
            if (mod.throwNative) throw std::runtime_error("oops");
            if (mod.throwUnknown) throw 10;
            reached++;
        }, [](std::string_view, std::string_view, const char*) {
            throw std::runtime_error("log unavailable");
        });
    REQUIRE(reached == 1);

    // Healthy-only reload/lifecycle pass should not generate diagnostics.
    std::map<std::string, SimulatedMod> reloaded{{"c_healthy", {}}};
    int initAgain = 0;
    m1core::scripts::ForEachModSafely(reloaded, "onInit",
        [&](const SimulatedMod&) { initAgain++; }, report);
    REQUIRE(initAgain == 1 && reports.size() == 8);

    // Empty mod list is also a valid lifecycle call.
    std::map<std::string, SimulatedMod> empty;
    m1core::scripts::ForEachModSafely(empty, "onShutdown",
        [&](const SimulatedMod&) { throw std::runtime_error("unreachable"); }, report);
    REQUIRE(reports.size() == 8);

    std::cout << "m1core M7: per-mod native lifecycle exception isolation and diagnostics passed\n";
}
