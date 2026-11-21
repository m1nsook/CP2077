#include "../core/src/scripting/ScriptFormats.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#define REQUIRE(expr) do { if (!(expr)) throw std::runtime_error("Test failed: " #expr); } while (false)

int main()
{
    using namespace m1core::scripts;
    namespace fs = std::filesystem;

    REQUIRE(Classify("init.m1s") == Format::Source);
    REQUIRE(Classify("init.lua") == Format::Source);
    REQUIRE(Classify("init.m1c") == Format::LuaJitBytecode);
    REQUIRE(Classify("init.M1C") == Format::LuaJitBytecode);
    REQUIRE(Classify("init.txt") == Format::Other);
    REQUIRE(IsLuaJitBytecode(std::string("\x1bLJ\x02", 4)));
    REQUIRE(!IsLuaJitBytecode(std::string("\x1bLua", 4)));
    REQUIRE(!IsLuaJitBytecode("print('not bytecode')"));
    REQUIRE(!IsLuaJitBytecode(std::string("\x1bLJ", 3)));

    const auto plain = Candidates("utils", false);
    REQUIRE(plain.size() == 4);
    REQUIRE(plain[0] == "utils");
    REQUIRE(plain[1] == "utils.m1s");
    REQUIRE(plain[2] == "utils.lua");
    REQUIRE(plain[3] == "utils.m1c");
    const auto module = Candidates("lib", true);
    REQUIRE(module.size() == 7);
    REQUIRE(module[4] == "lib/init.m1s");
    REQUIRE(module[5] == "lib/init.lua");
    REQUIRE(module[6] == "lib/init.m1c");
    REQUIRE(Candidates("lib.m1c", true).size() == 1);
    REQUIRE(Candidates("lib.lua", true).size() == 1);
    REQUIRE(Candidates("", true).empty());

    // Test the real filesystem resolution used by ScriptStore/ScriptContext.
    const auto root = fs::temp_directory_path() / "m1core-script-formats-test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto cleanup = [&] { fs::remove_all(root); };
    try
    {
        REQUIRE(FindEntrypoint(root).empty());
        { std::ofstream file(root / "init.m1c", std::ios::binary); file << "\x1bLJ\x02"; }
        REQUIRE(FindEntrypoint(root).filename() == "init.m1c");
        { std::ofstream file(root / "init.lua"); file << "return true"; }
        REQUIRE(FindEntrypoint(root).filename() == "init.lua");
        { std::ofstream file(root / "init.m1s"); file << "return true"; }
        REQUIRE(FindEntrypoint(root).filename() == "init.m1s");
        cleanup();
    }
    catch (...)
    {
        cleanup();
        throw;
    }
    std::cout << "m1core script format tests passed\n";
}
