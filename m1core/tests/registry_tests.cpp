#include "../native/registry.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using nlohmann::json;

int main()
{
    namespace fs = std::filesystem;
    const auto path = fs::temp_directory_path() / "m1core_registry_fresh_tests" / "settings.json";
    std::error_code ec;
    fs::remove_all(path.parent_path(), ec);
    m1core::StateStore store;
    store.Configure(path);
    assert(store.Ready());
    m1core::Registry registry(store);

    const auto schema = json::array({
        {{"id","active"},{"kind","boolean"},{"default",true},
         {"label","Enabled"},{"group","General"}},
        {{"id","capacity"},{"kind","integer"},{"min",0},{"max",250},
         {"default",50},{"group","Gameplay"}},
        {{"id","multiplier"},{"kind","float"},{"min",0.1},{"max",5.0},
         {"default",1.0}},
        {{"id","mode"},{"kind","choice"},{"choices",json::array({"auto","manual"})},
         {"default","auto"}}
    });
    assert(registry.Register("example","Demo Settings",schema));
    assert(registry.Snapshot().size()==1);
    assert(registry.Get("example","active").value()==true);
    assert(registry.Set("example","active",false));
    assert(registry.Get("example","active").value()==false);
    assert(registry.Set("example","capacity",10000));
    assert(registry.Get("example","capacity").value()==250);
    assert(!registry.Set("example","capacity","broken"));
    assert(registry.Set("example","mode","manual"));
    assert(!registry.Set("example","mode","no_such_value"));
    assert(registry.SaveProfile("stealth"));
    assert(registry.Set("example","active",true));
    assert(registry.LoadProfile("stealth"));
    assert(registry.Get("example","active").value()==false);
    assert(registry.Get("example","mode").value()=="manual");
    assert(registry.DeleteProfile("stealth"));
    assert(registry.Profiles().empty());
    assert(registry.Reset("example","capacity"));
    assert(registry.Get("example","capacity").value()==50);
    assert(registry.ResetMod("example"));
    assert(registry.Get("example","active").value()==true);
    assert(registry.Get("example","mode").value()=="auto");

    m1core::StateStore reopenStore;
    reopenStore.Configure(path);
    assert(reopenStore.Ready());
    m1core::Registry reopened(reopenStore);
    assert(reopened.Register("example","Demo Settings",schema));
    assert(reopened.Get("example","active").value()==true);
    assert(reopened.Get("example","capacity").value()==50);

    // Atomic descriptor rejection: any broken item rejects the whole mod.
    const auto invalid=json::array({
        {{"id","okay"},{"kind","boolean"},{"default",false}},
        {{"id","bad"},{"kind","choice"},{"choices",json::array({"x"})},
         {"default","wrong"}}
    });
    assert(!registry.Register("broken","Broken",invalid));
    assert(registry.Snapshot().size()==1);
    assert(!registry.Register("../evil","Evil",schema));

    fs::remove_all(path.parent_path(),ec);
    std::cout << "Native typed-settings registry tests passed\n";
}
