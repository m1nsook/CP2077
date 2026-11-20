#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <vector>
#include <string>
#include <string_view>
#include <variant>

namespace m1core
{
using Id = std::uint64_t;
using Value = std::variant<bool, std::int32_t, float>;

enum class Kind { Boolean, Integer, Float, Enum, Keybind };

struct Setting
{
    Kind kind;
    Value value;
    Value defaultValue;
    Value minimum;
    Value maximum;
    std::vector<std::string> choices;
    Id group = 0; // UI metadata only; zero means ungrouped.
    std::string displayName;
    std::string description;
    std::int32_t revision = 0; // Session-local token, not persisted.
};

// Immutable copy of registry state for UI rendering and diagnostics.
struct SettingSnapshot {
    Id id;
    Kind kind;
    Value value;
    Value defaultValue;
    Value minimum;
    Value maximum;
    std::vector<std::string> choices;
    Id group = 0; // UI metadata only; zero means ungrouped.
    std::string displayName;
    std::string description;
    std::int32_t revision = 0; // Session-local token, not persisted.
};
struct ModSnapshot {
    Id id;
    std::vector<SettingSnapshot> settings;
};

// No RED4ext dependencies here: persistence and settings logic are testable
// without starting the game or loading the native plugin.
class Registry
{
public:
    explicit Registry(std::filesystem::path file);

    bool Load();
    bool Save() const;

    // Named snapshots of all currently known values, including temporarily
    // unregistered mods. Profiles are stored independently of settings.json.
    bool LoadProfiles();
    bool SaveProfile(const std::string& name);
    bool ActivateProfile(const std::string& name);
    bool DeleteProfile(const std::string& name);
    // Atomic rename; cannot overwrite another profile or modify active values.
    bool RenameProfile(const std::string& oldName, const std::string& newName);
    std::vector<std::string> ListProfiles() const;

    bool RegisterBool(Id mod, Id id, bool defaultValue);
    bool RegisterInt(Id mod, Id id, std::int32_t defaultValue, std::int32_t minValue, std::int32_t maxValue);
    bool RegisterFloat(Id mod, Id id, float defaultValue, float minValue, float maxValue);
    // Enum is zero-based. Choices are UI labels; the numeric selection is persisted.
    bool RegisterEnum(Id mod, Id id, std::int32_t defaultIndex, std::vector<std::string> choices);
    // Win32 virtual-key code, 0 = unbound. Does not itself install gameplay hooks.
    bool RegisterKeybind(Id mod, Id id, std::int32_t defaultVk);

    std::optional<bool> GetBool(Id mod, Id id) const;
    std::optional<std::int32_t> GetInt(Id mod, Id id) const;
    std::optional<float> GetFloat(Id mod, Id id) const;
    std::optional<std::int32_t> GetEnum(Id mod, Id id) const;
    std::optional<std::int32_t> GetKeybind(Id mod, Id id) const;

    bool SetBool(Id mod, Id id, bool value);
    bool SetInt(Id mod, Id id, std::int32_t value);
    bool SetFloat(Id mod, Id id, float value);
    bool SetEnum(Id mod, Id id, std::int32_t index);
    bool SetKeybind(Id mod, Id id, std::int32_t virtualKey);

    bool Reset(Id mod, Id id);
    // Bulk resets update only registered values. Call Save() to persist.
    bool ResetMod(Id mod);
    bool ResetGroup(Id mod, Id group);
    bool SetGroup(Id mod, Id id, Id group);
    bool SetDisplayName(Id mod, Id id, std::string_view label);
    bool SetDescription(Id mod, Id id, std::string_view description);
    // 0 initially; token advances only for effective value changes.
    std::optional<std::int32_t> Revision(Id mod, Id id) const;
    std::vector<ModSnapshot> Snapshot() const;

private:
    using Settings = std::map<Id, Setting>;
    using Modules = std::map<Id, Settings>;
    using Values = std::map<Id, std::map<Id, std::pair<Kind, Value>>>;
    using Profiles = std::map<std::string, Values>;

    bool Register(Id mod, Id id, Setting setting);
    bool Set(Id mod, Id id, Kind kind, Value value);
    std::optional<Value> Get(Id mod, Id id, Kind kind) const;
    static Value Clamp(const Setting& setting, Value value);
    static bool UpdateValue(Setting& setting, Value value);
    std::filesystem::path ProfilesPath() const;
    bool WriteProfiles(const Profiles& profiles) const;

    std::filesystem::path file_;
    mutable std::mutex mutex_;
    Modules registered_;
    Values persisted_;
    Profiles profiles_;
};
} // namespace m1core
