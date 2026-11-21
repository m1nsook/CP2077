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
    // Unregistered/offline mods must also restore when loading the profile.
    auto edited = json::parse(store.Snapshot());
    edited["values"]["offline-mod"]["enabled"] = true;
    edited["values"]["offline-mod"]["newer_setting"] = 42;
    REQUIRE(store.Replace(edited.dump()));
    REQUIRE(registry.LoadProfile("stealth"));
    REQUIRE(registry.Get("example","active").value()==false);
    REQUIRE(registry.Get("example","mode").value()=="manual");
    const auto restored = json::parse(store.Snapshot());
    REQUIRE(restored["values"]["offline-mod"]["enabled"] == false);
    REQUIRE(restored["values"]["offline-mod"]["newer_setting"] == 42);
    // A mod registering after profile load must read its restored value.
    m1core::Registry lateRegistration(store);
    const auto offlineSchema = json::array({
        {{"id","enabled"},{"kind","boolean"},{"default",true}}
    });
    REQUIRE(lateRegistration.Register("offline-mod","Offline Mod",offlineSchema));
    REQUIRE(lateRegistration.Get("offline-mod","enabled").value()==false);

    // Missing/malformed profile loads must not mutate any values or
    // advance the persisted revision.
    REQUIRE(!registry.LoadProfile("missing"));
    REQUIRE(!registry.LoadProfile("../unsafe"));
    edited = json::parse(store.Snapshot());
    edited["profiles"]["invalid_known"] = {{"example",{{"active","not-a-bool"}}}};
    edited["profiles"]["invalid_unknown"] = {{"offline-mod","not-an-object"}};
    REQUIRE(store.Replace(edited.dump()));
    const auto beforeInvalid = store.Snapshot();
    const auto revisionBeforeInvalid = store.Revision();
    REQUIRE(!registry.LoadProfile("invalid_known"));
    REQUIRE(!registry.LoadProfile("invalid_unknown"));
    REQUIRE(store.Snapshot() == beforeInvalid);
    REQUIRE(store.Revision() == revisionBeforeInvalid);
    REQUIRE(registry.Get("example","active").value()==false);
    REQUIRE(registry.DeleteProfile("invalid_known"));
    REQUIRE(registry.DeleteProfile("invalid_unknown"));
    REQUIRE(!registry.DeleteProfile("../unsafe"));
    REQUIRE(!registry.DeleteProfile("missing"));
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
    m1core::Registry offlineRestart(reopenStore);
    REQUIRE(offlineRestart.Register("offline-mod","Offline Mod",offlineSchema));
    REQUIRE(offlineRestart.Get("offline-mod","enabled").value()==false);

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
