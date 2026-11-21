#include "../core/src/scripting/ScriptFormats.hpp"
#include <sol/sol.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#define REQUIRE(condition) do { if (!(condition)) throw std::runtime_error("LuaJIT test failed: " #condition); } while (false)

namespace fs = std::filesystem;

static int CaptureBytecode(lua_State*, const void* data, size_t len, void* result)
{
    auto& bytes = *static_cast<std::string*>(result);
    bytes.append(static_cast<const char*>(data), len);
    return 0;
}

static sol::object Execute(sol::state& vm, const fs::path& file)
{
    const auto format = m1core::scripts::Classify(file);
    REQUIRE(format != m1core::scripts::Format::Other);
    std::ifstream stream(file, std::ios::binary);
    REQUIRE(stream.good());
    std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    const bool binary = format == m1core::scripts::Format::LuaJitBytecode;
    if (binary) REQUIRE(m1core::scripts::IsLuaJitBytecode(bytes));
    else REQUIRE(!m1core::scripts::IsLuaJitBytecode(bytes));

    auto chunk = vm.load(bytes, "@" + file.string(),
        binary ? sol::load_mode::binary : sol::load_mode::text);
    REQUIRE(chunk.valid());
    sol::protected_function runner = chunk;
    sol::protected_function_result result = runner();
    REQUIRE(result.valid());
    return result.get<sol::object>();
}

int main(int argc, char** argv)
{
    REQUIRE(argc == 2);
    const fs::path fixture{argv[1]};
    REQUIRE(fs::is_directory(fixture));

    sol::state vm;
    vm.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table);

    const auto source = fixture / "init.m1s";
    REQUIRE(fs::is_regular_file(source));
    REQUIRE(m1core::scripts::FindEntrypoint(fixture) == source);
    int events = 0;
    vm.set_function("registerForEvent", [&](const std::string& event, sol::function callback) {
        REQUIRE(event == "onInit");
        sol::protected_function fn = callback;
        sol::protected_function_result called = fn();
        REQUIRE(called.valid());
        ++events;
    });
    vm.set_function("require", [&](const std::string& module) -> sol::object {
        for (const auto& candidate : m1core::scripts::Candidates(module, true))
        {
            const auto path = fixture / candidate;
            if (fs::is_regular_file(path))
                return Execute(vm, path);
        }
        throw std::runtime_error("Module does not exist: " + module);
    });
    Execute(vm, source);
    REQUIRE(events == 1);
    REQUIRE(vm["smoke_result"].get<int>() == 42);
    REQUIRE(Execute(vm, fixture / "helper.m1s").as<sol::table>()["answer"].get<int>() == 42);
    REQUIRE(Execute(vm, fixture / "compat.lua").as<int>() == 7);

    // Generate genuine bytecode using the same LuaJIT engine linked into
    // the m1core DLL. The bytecode test is NOT a header-only simulation.
    auto* state = vm.lua_state();
    REQUIRE(luaL_loadstring(state, "return 21 * 2") == 0);
    std::string compiled;
    REQUIRE(lua_dump(state, CaptureBytecode, &compiled, 0) == 0);
    lua_pop(state, 1);
    REQUIRE(m1core::scripts::IsLuaJitBytecode(compiled));
    const auto temporary = fs::temp_directory_path() / "m1core-luajit-integration.m1c";
    {
        std::ofstream output(temporary, std::ios::binary);
        REQUIRE(output.good());
        output.write(compiled.data(), static_cast<std::streamsize>(compiled.size()));
    }
    REQUIRE(Execute(vm, temporary).as<int>() == 42);
    REQUIRE(!vm.load("return 10", "@notbytecode.m1c", sol::load_mode::binary).valid());
    REQUIRE(!vm.load(compiled, "@masked.m1s", sol::load_mode::text).valid());
    fs::remove(temporary);
    std::cout << "m1core LuaJIT source, bytecode and module integration passed\n";
    return 0;
}
