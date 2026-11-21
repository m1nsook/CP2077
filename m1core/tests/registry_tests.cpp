#include "../native/registry.hpp"
#include "check.hpp"
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
    REQUIRE(store.Ready());
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
    REQUIRE(registry.Register("example","Demo Settings",schema));
    REQUIRE(registry.Snapshot().size()==1);
    REQUIRE(registry.Get("example","active").value()==true);
    REQUIRE(registry.Set("example","active",false));
    REQUIRE(registry.Get("example","active").value()==false);
    REQUIRE(registry.Set("example","capacity",10000));
    REQUIRE(registry.Get("example","capacity").value()==250);
    REQUIRE(!registry.Set("example","capacity","broken"));
    REQUIRE(registry.Set("example","mode","manual"));
    REQUIRE(!registry.Set("example","mode","no_such_value"));
    // Unregistered mod values must survive a profile save.
    auto persisted = json::parse(store.Snapshot());
    persisted["values"]["offline-mod"] = {{"enabled",false}};
    REQUIRE(store.Replace(persisted.dump()));
    REQUIRE(registry.SaveProfile("stealth"));
    const auto profileSaved = json::parse(store.Snapshot());
    REQUIRE(profileSaved["profiles"]["stealth"]["offline-mod"]["enabled"] == false);
    REQUIRE(profileSaved["profiles"]["stealth"]["example"]["active"] == false);
    REQUIRE(registry.Set("example","active",true));
    REQUIRE(registry.LoadProfile("stealth"));
    REQUIRE(registry.Get("example","active").value()==false);
    REQUIRE(registry.Get("example","mode").value()=="manual");
    REQUIRE(registry.DeleteProfile("stealth"));
    REQUIRE(registry.Profiles().empty());
    REQUIRE(registry.Reset("example","capacity"));
    REQUIRE(registry.Get("example","capacity").value()==50);
    REQUIRE(registry.ResetMod("example"));
    REQUIRE(registry.Get("example","active").value()==true);
    REQUIRE(registry.Get("example","mode").value()=="auto");

    m1core::StateStore reopenStore;
    reopenStore.Configure(path);
    REQUIRE(reopenStore.Ready());
    m1core::Registry reopened(reopenStore);
    REQUIRE(reopened.Register("example","Demo Settings",schema));
    REQUIRE(reopened.Get("example","active").value()==true);
    REQUIRE(reopened.Get("example","capacity").value()==50);

    // Atomic descriptor rejection: any broken item rejects the whole mod.
    const auto invalid=json::array({
        {{"id","okay"},{"kind","boolean"},{"default",false}},
        {{"id","bad"},{"kind","choice"},{"choices",json::array({"x"})},
         {"default","wrong"}}
    });
    REQUIRE(!registry.Register("broken","Broken",invalid));
    REQUIRE(registry.Snapshot().size()==1);
    REQUIRE(!registry.Register("../evil","Evil",schema));

    fs::remove_all(path.parent_path(),ec);
    std::cout << "Native typed-settings registry tests passed\n";
}
