#include "../native/state.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    namespace fs = std::filesystem;
    // Native state is the durable authority; the Lua registry is the UI
    // schema controller and never silently bypasses this DLL.
    const auto dir = fs::temp_directory_path() / "m1core_native_new_tests";
    fs::create_directories(dir);
    const auto path = dir / "settings.json";
    std::error_code ec;
    fs::remove(path, ec);

    m1core::StateStore store;
    store.Configure(path);
    assert(store.Ready());
    assert(store.Revision() == 0);
    assert(store.Replace(R"({"schema":1,"values":{"demo":{"enabled":false,"size":25}},"profiles":{}})"));
    assert(store.Revision() == 1);
    assert(store.Snapshot().find("\"enabled\":false") != std::string::npos);
    assert(!store.Replace("{this is not json"));
    assert(store.Revision() == 1);
    assert(!store.Replace(R"({"schema":2,"values":{},"profiles":{}})"));
    assert(store.Revision() == 1);

    m1core::StateStore restart;
    restart.Configure(path);
    assert(restart.Ready());
    assert(restart.Snapshot() == store.Snapshot());

    // Invalid files are never overwritten as a "repair".
    std::ofstream(path,std::ios::trunc) << "{ corrupt configuration";
    m1core::StateStore invalid;
    invalid.Configure(path);
    assert(!invalid.Ready());
    assert(!invalid.LastError().empty());
    assert(!invalid.Replace(R"({"schema":1,"values":{},"profiles":{}})"));
    std::ifstream keep(path);
    std::string content((std::istreambuf_iterator<char>(keep)), {});
    assert(content == "{ corrupt configuration");

    fs::remove_all(dir,ec);
    std::cout << "DLL native state tests passed\n";
    return 0;
}
