#include <chrono>
#include "../core/src/scripting/ModCompatibility.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define REQUIRE(x) do { if (!(x)) throw std::runtime_error("mod compatibility: " #x); } while (false)
int main()
{
    using namespace m1core::compat;
    const Version core{1,0,0};
    REQUIRE(ParseVersion("1.0.0").value() == core);
    REQUIRE(ParseVersion("1.0.5").value() > core);
    REQUIRE(ParseVersion("2.0.0").value() > ParseVersion("1.9.99").value());
    for (auto invalid : {"1.0","1.0.0-dev","1.0.0.1","1.02.0","1.0.-1","4294967296.0.0"})
        REQUIRE(!ParseVersion(invalid));

    const auto modernMod = nlohmann::json::parse(R"({
      "name":"Test Mod","version":"2.0.0","m1core":{"min":"1.0.0"}
    })");
    REQUIRE(!CheckJson(modernMod,"test",core).issue); // Mod 2.0.0 works on core 1.0.0.

    const auto newer = nlohmann::json::parse(R"({
      "name":"Updated Mod","version":"1.1.0","m1core":{"min":"1.0.5"}
    })");
    const auto blocked = CheckJson(newer,"updated",core);
    REQUIRE(blocked.issue && blocked.issue->reason == Reason::NeedsNewerCore);
    REQUIRE(blocked.issue->name == "Updated Mod");
    REQUIRE(blocked.issue->modVersion == "1.1.0");
    REQUIRE(blocked.issue->requiredMin == "1.0.5");
    REQUIRE(!CheckJson(newer,"updated",Version{1,0,5}).issue);

    const auto bounded = nlohmann::json::parse(R"({
      "version":"3.0.0","m1core":{"min":"1.0.0","max_exclusive":"2.0.0"}
    })");
    REQUIRE(!CheckJson(bounded,"bounded",Version{1,9,99}).issue);
    REQUIRE(CheckJson(bounded,"bounded",Version{2,0,0}).issue->reason == Reason::NeedsOlderCore);
    REQUIRE(CheckJson(bounded,"bounded",Version{0,9,0}).issue->reason == Reason::NeedsNewerCore);
    REQUIRE(CheckJson(nlohmann::json::parse(R"({
      "version":"1.0.0","m1core":{"min":"1.0.5","max_exclusive":"1.0.0"}
    })"),"bad",core).issue->reason == Reason::InvalidManifest);
    REQUIRE(CheckJson(nlohmann::json::parse(R"({"version":"2.0.0"})"),"missing",core).issue);

    const auto folder = std::filesystem::temp_directory_path() /
       ("m1core-version-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    REQUIRE(!ReadManifest(folder,"legacy",core).issue);
    { std::ofstream out(folder / "mod.json"); out << modernMod.dump(); }
    REQUIRE(!ReadManifest(folder,"test",core).issue);
    { std::ofstream out(folder / "mod.json",std::ios::trunc); out << newer.dump(); }
    REQUIRE(ReadManifest(folder,"updated",core).issue->reason == Reason::NeedsNewerCore);
    { std::ofstream out(folder / "mod.json",std::ios::trunc); out << "{ broken"; }
    REQUIRE(ReadManifest(folder,"broken",core).issue->reason == Reason::InvalidManifest);
    std::filesystem::remove_all(folder);
    std::cout << "m1core version: legacy, compatible, newer-core, upper bound and invalid manifests OK\n";
}
