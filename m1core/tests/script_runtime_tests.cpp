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
    return result.return_count() > 0 ? result.get<sol::object>() : sol::make_object(vm, sol::nil);
}

int main(int argc, char** argv)
{
    std::cerr << "LuaJIT test process started; argc=" << argc << std::endl;
    try
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
    sol::protected_function onInit;
    vm.set_function("registerForEvent", [&](const std::string& event, sol::function callback) {
        REQUIRE(event == "onInit");
        onInit = callback; // Actual lifecycle: register now, invoke later.
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
    std::cerr << "LuaJIT STEP 1: execute init.m1s" << std::endl;
    Execute(vm, source);
    REQUIRE(events == 1);
    REQUIRE(onInit.valid());
    std::cerr << "LuaJIT STEP 2: run registered onInit" << std::endl;
    sol::protected_function_result callbackResult = onInit();
    REQUIRE(callbackResult.valid());
    REQUIRE(vm["smoke_result"].get<int>() == 42);
    std::cerr << "LuaJIT STEP 3: execute .m1s and legacy .lua modules" << std::endl;
    sol::table module = Execute(vm, fixture / "helper.m1s");
    REQUIRE(module["answer"].get<int>() == 42);
    REQUIRE(Execute(vm, fixture / "compat.lua").as<int>() == 7);

    // Generate genuine bytecode using the same LuaJIT engine linked into
    // the m1core DLL. The bytecode test is NOT a header-only simulation.
    std::cerr << "LuaJIT STEP 4: compile real bytecode" << std::endl;
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
    std::cerr << "LuaJIT STEP 5: execute .m1c bytecode" << std::endl;
    REQUIRE(Execute(vm, temporary).as<int>() == 42);
    std::cerr << "LuaJIT STEP 6: reject wrong load modes" << std::endl;
    REQUIRE(!vm.load("return 10", "@notbytecode.m1c", sol::load_mode::binary).valid());
    REQUIRE(!vm.load(compiled, "@masked.m1s", sol::load_mode::text).valid());
    fs::remove(temporary);
    std::cout << "m1core LuaJIT source, bytecode and module integration passed\n";
    return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "LuaJIT integration failure: " << error.what() << std::endl;
        return 1;
    }
}
