#pragma once

#include "settings.hpp"

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace m1core
{
// Pure, renderer-independent diagnostics for user-assigned shortcuts.
// A duplicate is reported, never silently cleared or reserved by m1core.
struct KeybindOwner
{
    Id mod = 0;
    Id setting = 0;
};

struct KeybindConflict
{
    std::int32_t virtualKey = 0;
    std::vector<KeybindOwner> owners;
};

inline std::vector<KeybindConflict> FindKeybindConflicts(
    const std::vector<ModSnapshot>& mods)
{
    std::map<std::int32_t, std::vector<KeybindOwner>> assignments;
    for (const auto& mod : mods) {
        for (const auto& setting : mod.settings) {
            if (setting.kind != Kind::Keybind) continue;
            const auto* value = std::get_if<std::int32_t>(&setting.value);
            if (!value || *value <= 0 || *value > 255) continue;
            assignments[*value].push_back({mod.id, setting.id});
        }
    }
    std::vector<KeybindConflict> conflicts;
    for (auto& [key, owners] : assignments) {
        if (owners.size() > 1)
            conflicts.push_back({key, std::move(owners)});
    }
    return conflicts;
}
} // namespace m1core
