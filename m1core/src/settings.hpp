#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <vector>
#include <string>
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
    std::vector<ModSnapshot> Snapshot() const;

private:
    using Settings = std::map<Id, Setting>;
    using Modules = std::map<Id, Settings>;
    using Values = std::map<Id, std::map<Id, std::pair<Kind, Value>>>;

    bool Register(Id mod, Id id, Setting setting);
    bool Set(Id mod, Id id, Kind kind, Value value);
    std::optional<Value> Get(Id mod, Id id, Kind kind) const;
    static Value Clamp(const Setting& setting, Value value);

    std::filesystem::path file_;
    mutable std::mutex mutex_;
    Modules registered_;
    Values persisted_;
};
} // namespace m1core
