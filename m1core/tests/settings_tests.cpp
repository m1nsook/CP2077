#include "settings.hpp"

#include <cstdlib>
#include <iostream>

// Do not use assert: Release builds define NDEBUG and would skip the tests.
#define CHECK(expression) do { if (!(expression)) { \
    std::cerr << "Check failed: " #expression << " (line " << __LINE__ << ")\\n"; \
    std::exit(EXIT_FAILURE); \
} } while (false)
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "m1core-registry-tests";
    const auto file = directory / "settings.json";
    const auto profilesFile = directory / "profiles.json";
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);

    {
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.SetGroup(1, 1, 101));
        CHECK(!registry.SetGroup(1, 999, 101));
        CHECK(registry.RegisterBool(1, 1, true));

        CHECK(registry.RegisterBool(2, 1, false));
        CHECK(registry.RegisterInt(1, 2, 5, 0, 10));
        CHECK(registry.RegisterFloat(1, 3, 1.0F, 0.5F, 3.0F));
        CHECK(registry.RegisterEnum(1, 4, 1, {"Off", "Standard", "Stealth"}));
        CHECK(registry.RegisterKeybind(1, 5, 0x76)); // F7 virtual key.
        CHECK(!registry.RegisterEnum(1, 6, 0, {}));
        CHECK(!registry.RegisterEnum(1, 6, 0, {"Duplicate", "Duplicate"}));
        CHECK(!registry.RegisterKeybind(1, 6, 256));
        CHECK(!registry.RegisterBool(1, 2, false)); // Type collision.
        CHECK(!registry.RegisterFloat(1, 4, std::numeric_limits<float>::quiet_NaN(), 0, 1));
        CHECK(registry.SetBool(1, 1, false));
        CHECK(registry.SetInt(1, 2, 999));
        CHECK(registry.SetFloat(1, 3, -100.0F));
        CHECK(!registry.SetBool(1, 999, true));
        CHECK(!registry.SetFloat(1, 3, std::numeric_limits<float>::infinity()));
        CHECK(registry.SetEnum(1, 4, 42)); // Clamp to last choice.
        CHECK(registry.GetEnum(1, 4) == 2);
        CHECK(registry.SetKeybind(1, 5, 0)); // Unbound is valid.
        CHECK(!registry.SetKeybind(1, 5, 999));
        CHECK(registry.SetKeybind(1, 5, 0x72)); // F3.
        CHECK(!registry.SetInt(1, 4, 1)); // A typed enum is not a generic integer.
        CHECK(!registry.SetInt(1, 5, 1));
        CHECK(registry.GetBool(1, 1) == false);
        CHECK(registry.GetBool(2, 1) == false);
        CHECK(registry.GetInt(1, 2) == 10);
        CHECK(registry.GetFloat(1, 3) == 0.5F);
        CHECK(registry.GetEnum(1, 4) == 2);
        CHECK(registry.GetKeybind(1, 5) == 0x72);
        const auto snapshot = registry.Snapshot();
        CHECK(snapshot.size() == 2);
        CHECK(snapshot[0].id == 1);
        CHECK(snapshot[0].settings.size() == 5);
        CHECK(snapshot[0].settings[0].id == 1);
        CHECK(snapshot[0].settings[0].group == 101);
        CHECK(snapshot[0].settings[2].kind == m1core::Kind::Float);
        CHECK(std::get<float>(snapshot[0].settings[2].value) == 0.5F);
        CHECK(snapshot[0].settings[3].kind == m1core::Kind::Enum);
        CHECK(snapshot[0].settings[3].choices.size() == 3);
        CHECK(snapshot[0].settings[4].kind == m1core::Kind::Keybind);
        CHECK(registry.Save());
    }
    {
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.RegisterFloat(1, 3, 2.0F, 0.0F, 5.0F));
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.RegisterInt(1, 2, 0, 0, 20));
        CHECK(registry.RegisterEnum(1, 4, 0, {"Off", "Standard", "Stealth"}));
        CHECK(registry.RegisterKeybind(1, 5, 0x76));
        CHECK(registry.GetBool(1, 1) == false);
        CHECK(registry.GetInt(1, 2) == 10);
        CHECK(registry.GetFloat(1, 3) == 0.5F);
        CHECK(registry.Reset(1, 1));
        CHECK(registry.GetBool(1, 1) == true);
        CHECK(registry.Save());
    }
    {
        // CET-style passive startup: core/menu controls can be registered
        // before reading disk, then hydrated from saved values on gameplay
        // attach. Metadata and the chosen key must survive unchanged.
        m1core::Registry late(file);
        CHECK(late.RegisterKeybind(1, 5, 0));
        CHECK(late.SetDisplayName(1, 5, "Open / Close Menu"));
        CHECK(late.RegisterBool(1, 1, true));
        CHECK(late.GetKeybind(1, 5) == 0);
        CHECK(late.GetBool(1, 1) == true);
        CHECK(late.Load());
        CHECK(late.GetKeybind(1, 5) == 0x72); // Previously saved F3.
        CHECK(late.GetBool(1, 1) == true); // Reset in the prior block.
        CHECK(late.Revision(1, 5) == 1);
        const auto snap = late.Snapshot();
        CHECK(snap[0].settings[4].displayName == "Open / Close Menu");
        CHECK(late.Save());
    }

    {
        // An inactive module must not lose its saved values.
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.RegisterBool(2, 1, true));
        CHECK(registry.GetBool(2, 1) == false);
    }

    {
        // Profile names are JSON keys, never filesystem paths. Reject names
        // with separators, controls, empty/oversized values, and bad spacing.
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.LoadProfiles());
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.RegisterEnum(1, 4, 1, {"Off", "Standard", "Stealth"}));
        CHECK(registry.RegisterKeybind(1, 5, 118));
        CHECK(!registry.SaveProfile(""));
        CHECK(!registry.SaveProfile("/invalid"));
        CHECK(!registry.SaveProfile(" bad "));
        CHECK(!registry.SaveProfile(std::string(65, 'x')));
        CHECK(registry.SaveProfile("Stealth"));

        CHECK(registry.SetBool(1, 1, false));
        CHECK(registry.SetEnum(1, 4, 0));
        CHECK(registry.SetKeybind(1, 5, 119));
        CHECK(registry.SaveProfile("Combat"));
        CHECK(registry.ActivateProfile("Stealth"));
        CHECK(registry.GetBool(1, 1) == true);
        CHECK(registry.GetEnum(1, 4) == 2); // Saved enum came from original file.
        CHECK(registry.GetKeybind(1, 5) == 0x72); // Original saved F3.
        CHECK(registry.Save());
        CHECK(!registry.ActivateProfile("Unknown"));
        const auto names = registry.ListProfiles();
        CHECK(names.size() == 2);
        CHECK(names[0] == "Combat");
        CHECK(names[1] == "Stealth");
    }
    {
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.LoadProfiles());
        CHECK(registry.ListProfiles().size() == 2);
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.RegisterEnum(1, 4, 1, {"Off", "Standard", "Stealth"}));
        CHECK(registry.RegisterKeybind(1, 5, 118));
        CHECK(registry.ActivateProfile("Combat"));
        CHECK(registry.GetBool(1, 1) == false);
        CHECK(registry.GetEnum(1, 4) == 0);
        CHECK(registry.GetKeybind(1, 5) == 119);
        CHECK(registry.DeleteProfile("Stealth"));
        CHECK(!registry.DeleteProfile("Stealth"));
        CHECK(registry.ListProfiles().size() == 1);
        CHECK(registry.Save());
    }
    {
        m1core::Registry registry(file);
        CHECK(registry.Load());
        CHECK(registry.LoadProfiles());
        CHECK(registry.ListProfiles().size() == 1);
        // Loading a profile with a shorter enum safely clamps its index.
        CHECK(registry.RegisterEnum(1, 4, 1, {"Off", "Standard"}));
        CHECK(registry.ActivateProfile("Combat"));
        CHECK(registry.GetEnum(1, 4) == 0);
        CHECK(registry.GetEnum(1, 4) != std::nullopt);
    }
    {
        // Bulk resets affect only the requested mod/group and persist
        // existing schema-v1 values through Save/Load without touching peers.
        const auto bulkFile = directory / "bulk.json";
        m1core::Registry registry(bulkFile);
        CHECK(registry.Load());
        CHECK(registry.RegisterBool(10, 1, true));
        CHECK(registry.RegisterInt(10, 2, 3, 0, 10));
        CHECK(registry.RegisterKeybind(10, 3, 65));
        CHECK(registry.RegisterBool(20, 1, true));
        CHECK(registry.SetGroup(10, 1, 100));
        CHECK(registry.SetGroup(10, 2, 100));
        CHECK(registry.SetBool(10, 1, false));
        CHECK(registry.SetInt(10, 2, 9));
        CHECK(registry.SetKeybind(10, 3, 66));
        CHECK(registry.SetBool(20, 1, false));
        CHECK(!registry.ResetGroup(10, 999));
        CHECK(!registry.ResetMod(999));
        CHECK(registry.ResetGroup(10, 100));
        CHECK(registry.GetBool(10, 1) == true);
        CHECK(registry.GetInt(10, 2) == 3);
        CHECK(registry.GetKeybind(10, 3) == 66);
        CHECK(registry.GetBool(20, 1) == false);
        CHECK(registry.ResetGroup(10, 0)); // Ungrouped.
        CHECK(registry.GetKeybind(10, 3) == 65);
        CHECK(registry.SetInt(10, 2, 8));
        CHECK(registry.SetBool(10, 1, false));
        CHECK(registry.ResetMod(10));
        CHECK(registry.GetBool(10, 1) == true);
        CHECK(registry.GetInt(10, 2) == 3);
        CHECK(registry.GetBool(20, 1) == false);
        CHECK(registry.Save());
        m1core::Registry reloaded(bulkFile);
        CHECK(reloaded.Load());
        CHECK(reloaded.RegisterBool(10, 1, false));
        CHECK(reloaded.RegisterBool(20, 1, true));
        CHECK(reloaded.GetBool(10, 1) == true);
        CHECK(reloaded.GetBool(20, 1) == false);
    }

    {
        // M5: per-setting revisions change only when effective values change.
        // Display metadata never pollutes schema-v1 JSON or profiles.
        const auto metaDir = directory / "metadata";
        const auto metaFile = metaDir / "settings.json";
        m1core::Registry registry(metaFile);
        CHECK(registry.Load());
        CHECK(!registry.SetDisplayName(7, 8, "Unregistered"));
        CHECK(registry.RegisterBool(7, 8, true));
        CHECK(registry.RegisterBool(9, 8, true));
        CHECK(registry.RegisterInt(7, 9, 3, 0, 5));
        CHECK(registry.Revision(7, 8) == 0);
        CHECK(registry.Revision(7, 99) == std::nullopt);
        CHECK(registry.SetDisplayName(7, 8, "Feature enabled"));
        CHECK(registry.SetDescription(7, 8, "Enable the feature for this mod."));
        CHECK(!registry.SetDescription(7, 8, std::string(513, 'x')));
        CHECK(!registry.SetDisplayName(7, 8, "Invalid\nlabel"));
        CHECK(!registry.SetDisplayName(7, 8, std::string(97, 'x')));
        CHECK(registry.SetBool(7, 8, true)); // No effective change.
        CHECK(registry.Revision(7, 8) == 0);
        CHECK(registry.SetBool(7, 8, false));
        CHECK(registry.Revision(7, 8) == 1);
        CHECK(registry.SetBool(7, 8, false));
        CHECK(registry.Revision(7, 8) == 1);
        CHECK(registry.Reset(7, 8));
        CHECK(registry.Revision(7, 8) == 2);
        CHECK(registry.ResetMod(7)); // Already at defaults.
        CHECK(registry.Revision(7, 8) == 2);
        CHECK(registry.Revision(9, 8) == 0); // Isolated mod.
        CHECK(registry.SetInt(7, 9, 999)); // Clamped to 5.
        CHECK(registry.Revision(7, 9) == 1);
        CHECK(registry.SetInt(7, 9, 999)); // Still 5.
        CHECK(registry.Revision(7, 9) == 1);
        CHECK(registry.SaveProfile("Modified"));
        CHECK(registry.ResetMod(7));
        CHECK(registry.Revision(7, 9) == 2);
        CHECK(registry.ActivateProfile("Modified"));
        CHECK(registry.Revision(7, 9) == 3);
        CHECK(registry.ActivateProfile("Modified"));
        CHECK(registry.Revision(7, 9) == 3);
        CHECK(registry.RegisterBool(7, 8, true)); // Same-ID registration preserves metadata.
        auto snapshot = registry.Snapshot();
        CHECK(snapshot[0].settings[0].displayName == "Feature enabled");
        CHECK(snapshot[0].settings[0].description == "Enable the feature for this mod.");
        CHECK(snapshot[0].settings[0].revision == 2);
        CHECK(registry.RegisterInt(7, 9, 3, 0, 4)); // New bounds clamp 5 to 4.
        CHECK(registry.GetInt(7, 9) == 4);
        CHECK(registry.Revision(7, 9) == 4);
        CHECK(registry.Save());

        m1core::Registry restarted(metaFile);
        CHECK(restarted.Load());
        CHECK(restarted.RegisterBool(7, 8, false));
        CHECK(restarted.RegisterInt(7, 9, 0, 0, 5));
        CHECK(restarted.GetBool(7, 8) == true);
        CHECK(restarted.GetInt(7, 9) == 4);
        CHECK(restarted.Revision(7, 8) == 0);
        CHECK(restarted.Revision(7, 9) == 0);
        snapshot = restarted.Snapshot();
        CHECK(snapshot[0].settings[0].displayName.empty());
        CHECK(snapshot[0].settings[0].description.empty());
    }

    {
        const auto renameFile = directory / "rename" / "settings.json";
        m1core::Registry registry(renameFile);
        CHECK(registry.Load());
        CHECK(registry.LoadProfiles());
        CHECK(registry.RegisterBool(12, 1, true));
        CHECK(registry.SaveProfile("Stealth"));
        CHECK(registry.SetBool(12, 1, false));
        CHECK(registry.SaveProfile("Combat"));
        CHECK(!registry.RenameProfile("Combat", "Stealth"));
        CHECK(!registry.RenameProfile("Unknown", "Explorer"));
        CHECK(!registry.RenameProfile("Combat", " bad "));
        CHECK(!registry.RenameProfile("Combat", "Combat"));
        CHECK(registry.RenameProfile("Combat", "Explorer"));
        CHECK(!registry.ActivateProfile("Combat"));
        CHECK(registry.GetBool(12, 1) == false);
        CHECK(registry.ListProfiles().size() == 2);
        CHECK(registry.ActivateProfile("Stealth"));
        CHECK(registry.GetBool(12, 1) == true);
        m1core::Registry reloaded(renameFile);
        CHECK(reloaded.LoadProfiles());
        CHECK(reloaded.ListProfiles().size() == 2);
        CHECK(reloaded.RegisterBool(12, 1, true));
        CHECK(reloaded.ActivateProfile("Explorer"));
        CHECK(reloaded.GetBool(12, 1) == false);
    }

    {
        std::ofstream output(profilesFile, std::ios::trunc);
        output << "{ damaged profiles";
    }
    {
        // An invalid profiles.json cannot break the existing settings.json.
        m1core::Registry registry(file);
        CHECK(!registry.LoadProfiles());
        CHECK(registry.Load());
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.GetBool(1, 1) == false);
    }

    {
        std::ofstream output(file, std::ios::trunc);
        output << "{ corrupt json";
    }
    {
        m1core::Registry registry(file);
        CHECK(!registry.Load());
        CHECK(registry.RegisterBool(1, 1, true));
        CHECK(registry.GetBool(1, 1) == true);
    }
    std::filesystem::remove_all(directory, ignored);
}
