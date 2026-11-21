#include "../core/src/scripting/M1CoreLuaAPI.hpp"
#include "../core/src/scripting/ScriptFormats.hpp"
#include "../native/registry.hpp"
#include "../native/state.hpp"

#include <sol/sol.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#define CHECK(expr) do { if (!(expr)) throw std::runtime_error("weight limit test: " #expr); } while (false)
namespace fs = std::filesystem;

static std::string Read(const fs::path& file)
{
    std::ifstream stream(file, std::ios::binary);
    CHECK(stream.good());
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

static int DumpWriter(lua_State*, const void* bytes, size_t length, void* destination)
{
    static_cast<std::string*>(destination)->append(static_cast<const char*>(bytes), length);
    return 0;
}

static std::string CompileLuaJIT(const fs::path& source)
{
    sol::state compiler;
    compiler.open_libraries(sol::lib::base);
    auto* state = compiler.lua_state();
    const auto input = source.string();
    if (luaL_loadfile(state, input.c_str()) != 0)
        throw std::runtime_error(lua_tostring(state, -1));
    std::string output;
    CHECK(lua_dump(state, DumpWriter, &output, 0) == 0);
    lua_pop(state, 1);
    CHECK(m1core::scripts::IsLuaJitBytecode(output));
    return output;
}

struct SimulatedGame
{
    sol::state vm;
    sol::table player;
    sol::table system;
    sol::table game;
    sol::protected_function initialize;
    sol::protected_function update;
    sol::protected_function shutdown;

    double base = 210.0;
    double bonus = 0.0;
    int additions = 0;
    int removals = 0;
    bool playerPresent = false;

    SimulatedGame()
    {
        vm.open_libraries(sol::lib::base, sol::lib::math,
                          sol::lib::string, sol::lib::table);
        // The game bridge populates a sol::table sandbox base, not the
        // sol::global_table wrapper returned by state::globals().
        sol::table sandboxGlobals = vm.create_table();
        m1core::BindLuaRegistryAPI(vm, sandboxGlobals);
        vm["m1core"] = sandboxGlobals["m1core"];

        vm.set_function("registerForEvent", [this](const std::string& event, sol::function callback) {
            if (event == "onInit") initialize = sol::protected_function(callback);
            else if (event == "onUpdate") update = sol::protected_function(callback);
            else if (event == "onShutdown") shutdown = sol::protected_function(callback);
            else throw std::runtime_error("Unexpected Lua event: " + event);
        });

        player = vm.create_table();
        player.set_function("GetEntityID", [](sol::table) { return 42; });

        system = vm.create_table();
        system.set_function("GetStatValue", [this](sol::table, int playerId, const std::string& name) {
            CHECK(playerId == 42 && name == "CarryCapacity");
            return base + bonus;
        });
        system.set_function("AddModifier", [this](sol::table, int playerId, sol::table modifier) {
            CHECK(playerId == 42);
            bonus += modifier["delta"].get<double>();
            ++additions;
        });
        system.set_function("RemoveModifier", [this](sol::table, int playerId, sol::table modifier) {
            CHECK(playerId == 42);
            bonus -= modifier["delta"].get<double>();
            ++removals;
        });

        game = vm.create_table();
        game.set_function("GetPlayer", [this]() -> sol::object {
            return playerPresent ? sol::make_object(vm, player)
                                 : sol::make_object(vm, sol::nil);
        });
        game.set_function("GetStatsSystem", [this]() -> sol::table { return system; });
        game.set_function("gameRPGManager::CreateStatModifier;gamedataStatTypegameStatModifierTypeFloat",
            [this](const std::string& stat, const std::string& kind, double delta) -> sol::table {
                CHECK(stat == "CarryCapacity" && kind == "Additive");
                sol::table modifier = vm.create_table();
                modifier["delta"] = delta;
                return modifier;
            });
        vm["Game"] = game;
    }

    void Load(const std::string& bytes, bool binary)
    {
        const auto mode = binary ? sol::load_mode::binary : sol::load_mode::text;
        auto chunk = vm.load(bytes, binary ? "@init.m1c" : "@init.m1s", mode);
        if (!chunk.valid())
            throw std::runtime_error(chunk.get<sol::error>().what());
        sol::protected_function script = chunk;
        auto result = script();
        if (!result.valid())
            throw std::runtime_error(result.get<sol::error>().what());
        CHECK(initialize.valid() && update.valid() && shutdown.valid());
    }

    void Init()
    {
        auto result = initialize();
        if (!result.valid()) throw std::runtime_error(result.get<sol::error>().what());
    }

    void Tick()
    {
        auto result = update(0.6);
        if (!result.valid()) throw std::runtime_error(result.get<sol::error>().what());
    }

    void Stop()
    {
        auto result = shutdown();
        if (!result.valid()) throw std::runtime_error(result.get<sol::error>().what());
    }

    void SetJson(const std::string& setting, const std::string& json)
    {
        sol::table api = vm["m1core"];
        sol::protected_function setter = api["setJson"];
        auto result = setter("m1core.weight_limit_sample", setting, json);
        CHECK(result.valid());
        CHECK(result.get<bool>());
    }

    double Current() const { return base + bonus; }
};

static void TestConsumer(const std::string& script, bool binary, const fs::path& root)
{
    const auto label = binary ? "bytecode" : "source";
    m1core::Store().Configure(root / (std::string(label) + ".json"));
    SimulatedGame test;
    test.Load(script, binary);
    test.Init();
    CHECK(m1core::Settings().Get("m1core.weight_limit_sample", "limit").value() == 500);
    CHECK(test.additions == 0); // Nothing applied while player is absent.

    test.playerPresent = true;
    test.Tick();
    CHECK(std::abs(test.Current() - 500.0) < 0.01);
    CHECK(test.additions == 1);

    test.Tick();
    CHECK(test.additions == 1); // Never stack duplicate modifiers.

    test.SetJson("limit", "850");
    test.Tick();
    CHECK(std::abs(test.Current() - 850.0) < 0.01);
    CHECK(test.additions == 2 && test.removals == 1);

    test.SetJson("enabled", "false");
    test.Tick();
    CHECK(std::abs(test.Current() - test.base) < 0.01);
    CHECK(test.removals == 2);

    test.SetJson("enabled", "true");
    test.Tick();
    CHECK(std::abs(test.Current() - 850.0) < 0.01);
    CHECK(test.additions == 3);

    test.Stop();
    CHECK(std::abs(test.Current() - test.base) < 0.01);
    CHECK(test.removals == 3);
    std::cout << "m1core weight-limit " << label << ": settings + carry-capacity lifecycle passed\n";
}

int main(int argc, char** argv)
{
    try
    {
        CHECK(argc == 3);
        const fs::path source{argv[1]};
        const fs::path output{argv[2]};
        CHECK(m1core::scripts::Classify(source) == m1core::scripts::Format::Source);
        CHECK(m1core::scripts::Classify(output) == m1core::scripts::Format::LuaJitBytecode);
        const auto text = Read(source);
        CHECK(!m1core::scripts::IsLuaJitBytecode(text));
        const auto bytecode = CompileLuaJIT(source);
        fs::create_directories(output.parent_path());
        {
            std::ofstream compiled(output, std::ios::binary | std::ios::trunc);
            CHECK(compiled.good());
            compiled.write(bytecode.data(), static_cast<std::streamsize>(bytecode.size()));
            CHECK(compiled.good());
        }
        CHECK(Read(output) == bytecode);

        const fs::path scratch = fs::temp_directory_path() / "m1core-weight-limit-test";
        fs::create_directories(scratch);
        fs::remove(scratch / "source.json");
        fs::remove(scratch / "bytecode.json");
        TestConsumer(text, false, scratch);
        TestConsumer(bytecode, true, scratch);

        fs::remove(scratch / "source.json");
        fs::remove(scratch / "bytecode.json");
        std::cout << "m1core Lua bridge and actual LuaJIT .m1s/.m1c example passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "m1core weight limit test failed: " << error.what() << '\n';
        return 1;
    }
}
