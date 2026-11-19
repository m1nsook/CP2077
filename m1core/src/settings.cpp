#include "settings.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace m1core
{
namespace
{
using json = nlohmann::json;

std::string Key(Id id)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << id;
    return out.str();
}

std::optional<Id> ParseKey(const std::string& text)
{
    if (text.size() != 16)
        return std::nullopt;
    for (char c : text)
    {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return std::nullopt;
    }
    try
    {
        return std::stoull(text, nullptr, 16);
    }
    catch (...)
    {
        return std::nullopt;
    }
}

const char* Name(Kind kind)
{
    switch (kind)
    {
    case Kind::Boolean: return "bool";
    case Kind::Integer: return "int";
    case Kind::Float: return "float";
    case Kind::Enum: return "enum";
    case Kind::Keybind: return "keybind";
    }
    return "unknown";
}

std::optional<std::pair<Kind, Value>> Decode(const json& setting)
{
    if (!setting.is_object() || !setting.contains("type") || !setting.contains("value") ||
        !setting["type"].is_string())
        return std::nullopt;

    const auto& type = setting["type"].get_ref<const std::string&>();
    const auto& value = setting["value"];
    if (type == "bool" && value.is_boolean())
        return std::pair{Kind::Boolean, Value(value.get<bool>())};
    if ((type == "int" || type == "enum" || type == "keybind") && value.is_number_integer())
    {
        const auto number = value.get<std::int64_t>();
        if (number >= std::numeric_limits<std::int32_t>::min() &&
            number <= std::numeric_limits<std::int32_t>::max())
            return std::pair{type == "enum" ? Kind::Enum : type == "keybind" ? Kind::Keybind : Kind::Integer,
                             Value(static_cast<std::int32_t>(number))};
    }
    if (type == "float" && value.is_number())
    {
        const auto number = value.get<double>();
        if (std::isfinite(number) &&
            number >= -std::numeric_limits<float>::max() &&
            number <= std::numeric_limits<float>::max())
            return std::pair{Kind::Float, Value(static_cast<float>(number))};
    }
    return std::nullopt;
}

json Encode(Kind kind, const Value& value)
{
    json encoded = {{"type", Name(kind)}};
    std::visit([&encoded](const auto& v) { encoded["value"] = v; }, value);
    return encoded;
}

bool ValidProfileName(std::string_view name)
{
    if (name.empty() || name.size() > 64 || name.front() == ' ' || name.back() == ' ')
        return false;
    for (unsigned char byte : name)
        if (byte < 0x20 || byte == 0x7f || byte == '/' || byte == '\\')
            return false;
    return true;
}

bool WriteJsonAtomic(const std::filesystem::path& file, const json& doc)
{
    try
    {
        std::filesystem::create_directories(file.parent_path());
        auto temporary = file;
        temporary += ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return false;
            output << doc.dump(2) << '\n';
            output.flush();
            if (!output) return false;
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), file.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
#else
        std::filesystem::rename(temporary, file);
#endif
        return true;
    }
    catch (...) { return false; }
}
} // namespace

Registry::Registry(std::filesystem::path file) : file_(std::move(file)) {}

bool Registry::Load()
{
    try
    {
        if (!std::filesystem::exists(file_))
        {
            std::lock_guard lock(mutex_);
            persisted_.clear();
            return true;
        }

        std::ifstream input(file_, std::ios::binary);
        if (!input)
            return false;

        const auto doc = json::parse(input);
        if (!doc.is_object() || doc.value("schemaVersion", 0) != 1 ||
            !doc.contains("mods") || !doc["mods"].is_object())
            return false;

        Values loaded;
        for (const auto& [modText, values] : doc["mods"].items())
        {
            const auto modId = ParseKey(modText);
            if (!modId || !values.is_object())
                continue;
            for (const auto& [idText, item] : values.items())
            {
                const auto settingId = ParseKey(idText);
                const auto parsed = Decode(item);
                if (settingId && parsed)
                    loaded[*modId][*settingId] = *parsed;
            }
        }

        std::lock_guard lock(mutex_);
        persisted_ = std::move(loaded);
        return true;
    }
    catch (...)
    {
        // A damaged configuration must not crash game startup.
        return false;
    }
}

bool Registry::Save() const
{
    try
    {
        Values snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot = persisted_; // Keep values belonging to currently absent mods.
            for (const auto& [modId, settings] : registered_)
                for (const auto& [id, setting] : settings)
                    snapshot[modId][id] = {setting.kind, setting.value};
        }

        json doc = {{"schemaVersion", 1}, {"mods", json::object()}};
        for (const auto& [modId, settings] : snapshot)
        {
            auto& mod = doc["mods"][Key(modId)];
            mod = json::object();
            for (const auto& [id, setting] : settings)
                mod[Key(id)] = Encode(setting.first, setting.second);
        }

        return WriteJsonAtomic(file_, doc);
    }
    catch (...)
    {
        return false;
    }
}

std::filesystem::path Registry::ProfilesPath() const
{
    auto profiles = file_;
    profiles.replace_filename("profiles.json");
    return profiles;
}

bool Registry::WriteProfiles(const Profiles& profiles) const
{
    json doc = {{"schemaVersion", 1}, {"profiles", json::object()}};
    for (const auto& [name, mods] : profiles)
    {
        auto& profile = doc["profiles"][name];
        profile = json::object();
        for (const auto& [modId, settings] : mods)
        {
            auto& mod = profile[Key(modId)];
            mod = json::object();
            for (const auto& [id, value] : settings)
                mod[Key(id)] = Encode(value.first, value.second);
        }
    }
    return WriteJsonAtomic(ProfilesPath(), doc);
}

bool Registry::LoadProfiles()
{
    try
    {
        const auto path = ProfilesPath();
        if (!std::filesystem::exists(path))
        {
            std::lock_guard lock(mutex_);
            profiles_.clear();
            return true;
        }
        // A damaged or unexpectedly large file must not stop game startup.
        if (std::filesystem::file_size(path) > 4 * 1024 * 1024)
            return false;
        std::ifstream input(path, std::ios::binary);
        if (!input) return false;
        const auto doc = json::parse(input);
        if (!doc.is_object() || !doc.contains("schemaVersion") ||
            !doc["schemaVersion"].is_number_integer() ||
            doc["schemaVersion"].get<int>() != 1 ||
            !doc.contains("profiles") || !doc["profiles"].is_object())
            return false;

        Profiles loaded;
        for (const auto& [name, modules] : doc["profiles"].items())
        {
            if (!ValidProfileName(name) || !modules.is_object())
                continue;
            if (loaded.size() >= 24)
                break;
            Values values;
            for (const auto& [modText, settings] : modules.items())
            {
                const auto modId = ParseKey(modText);
                if (!modId || !settings.is_object()) continue;
                for (const auto& [idText, entry] : settings.items())
                {
                    const auto id = ParseKey(idText);
                    const auto decoded = Decode(entry);
                    if (id && decoded)
                        values[*modId][*id] = *decoded;
                }
            }
            if (!values.empty())
                loaded.emplace(name, std::move(values));
        }
        std::lock_guard lock(mutex_);
        profiles_ = std::move(loaded);
        return true;
    }
    catch (...) { return false; }
}

bool Registry::SaveProfile(const std::string& name)
{
    if (!ValidProfileName(name)) return false;
    std::lock_guard lock(mutex_);
    if (!profiles_.contains(name) && profiles_.size() >= 24)
        return false;
    Values values = persisted_;
    // Overlay the latest values for registered mods, not stale disk state.
    for (const auto& [modId, settings] : registered_)
        for (const auto& [id, setting] : settings)
            values[modId][id] = {setting.kind, setting.value};
    if (values.empty())
        return false;

    auto next = profiles_;
    next[name] = std::move(values);
    if (!WriteProfiles(next))
        return false;
    profiles_ = std::move(next);
    return true;
}

bool Registry::ActivateProfile(const std::string& name)
{
    std::lock_guard lock(mutex_);
    const auto found = profiles_.find(name);
    if (found == profiles_.end()) return false;
    for (const auto& [modId, settings] : found->second)
        for (const auto& [id, stored] : settings)
        {
            const auto module = registered_.find(modId);
            if (module != registered_.end())
            {
                const auto current = module->second.find(id);
                if (current != module->second.end())
                {
                    // Never corrupt an existing setting whose type changed.
                    if (current->second.kind != stored.first)
                        continue;
                    current->second.value = Clamp(current->second, stored.second);
                    persisted_[modId][id] = {stored.first, current->second.value};
                    continue;
                }
            }
            // Preserve values for mods that are installed but do not
            // currently register settings; they take effect on registration.
            persisted_[modId][id] = stored;
        }
    return true;
}

bool Registry::DeleteProfile(const std::string& name)
{
    std::lock_guard lock(mutex_);
    if (!profiles_.contains(name)) return false;
    auto next = profiles_;
    next.erase(name);
    if (!WriteProfiles(next)) return false;
    profiles_ = std::move(next);
    return true;
}

std::vector<std::string> Registry::ListProfiles() const
{
    std::lock_guard lock(mutex_);
    std::vector<std::string> names;
    names.reserve(profiles_.size());
    for (const auto& [name, _] : profiles_)
        names.push_back(name);
    return names;
}

Value Registry::Clamp(const Setting& setting, Value value)
{
    if (setting.kind == Kind::Integer || setting.kind == Kind::Enum || setting.kind == Kind::Keybind)
        return std::clamp(std::get<std::int32_t>(value),
                          std::get<std::int32_t>(setting.minimum),
                          std::get<std::int32_t>(setting.maximum));
    if (setting.kind == Kind::Float)
        return std::clamp(std::get<float>(value),
                          std::get<float>(setting.minimum),
                          std::get<float>(setting.maximum));
    return value;
}

bool Registry::Register(Id mod, Id id, Setting setting)
{
    if (!mod || !id)
        return false;

    std::lock_guard lock(mutex_);
    auto& module = registered_[mod];
    if (const auto it = module.find(id); it != module.end())
    {
        if (it->second.kind != setting.kind)
            return false;
        setting.value = Clamp(setting, it->second.value);
    }
    else if (const auto loadedMod = persisted_.find(mod); loadedMod != persisted_.end())
    {
        if (const auto loaded = loadedMod->second.find(id); loaded != loadedMod->second.end() &&
            loaded->second.first == setting.kind)
            setting.value = Clamp(setting, loaded->second.second);
    }
    module[id] = std::move(setting);
    return true;
}

bool Registry::RegisterBool(Id mod, Id id, bool def)
{
    return Register(mod, id, {Kind::Boolean, def, def, false, true});
}

bool Registry::RegisterInt(Id mod, Id id, std::int32_t def, std::int32_t min, std::int32_t max)
{
    if (min > max)
        return false;
    def = std::clamp(def, min, max);
    return Register(mod, id, {Kind::Integer, def, def, min, max});
}

bool Registry::RegisterFloat(Id mod, Id id, float def, float min, float max)
{
    if (!std::isfinite(def) || !std::isfinite(min) || !std::isfinite(max) || min > max)
        return false;
    def = std::clamp(def, min, max);
    return Register(mod, id, {Kind::Float, def, def, min, max});
}

bool Registry::RegisterEnum(Id mod, Id id, std::int32_t def,
                            std::vector<std::string> choices)
{
    if (choices.empty() || choices.size() > 64)
        return false;
    for (const auto& choice : choices)
        if (choice.empty() || choice.size() > 128)
            return false;
    // Avoid ambiguous labels when rendering selection changes.
    for (std::size_t i = 0; i < choices.size(); ++i)
        for (std::size_t j = i + 1; j < choices.size(); ++j)
            if (choices[i] == choices[j]) return false;
    const auto maximum = static_cast<std::int32_t>(choices.size() - 1);
    def = std::clamp(def, 0, maximum);
    return Register(mod, id, {Kind::Enum, def, def, 0, maximum, std::move(choices)});
}

bool Registry::RegisterKeybind(Id mod, Id id, std::int32_t def)
{
    if (def < 0 || def > 255)
        return false;
    return Register(mod, id, {Kind::Keybind, def, def, 0, 255});
}

std::optional<Value> Registry::Get(Id mod, Id id, Kind kind) const
{
    std::lock_guard lock(mutex_);
    const auto module = registered_.find(mod);
    if (module == registered_.end())
        return std::nullopt;
    const auto setting = module->second.find(id);
    if (setting == module->second.end() || setting->second.kind != kind)
        return std::nullopt;
    return setting->second.value;
}

std::optional<bool> Registry::GetBool(Id mod, Id id) const
{
    const auto value = Get(mod, id, Kind::Boolean);
    return value ? std::optional(std::get<bool>(*value)) : std::nullopt;
}

std::optional<std::int32_t> Registry::GetInt(Id mod, Id id) const
{
    const auto value = Get(mod, id, Kind::Integer);
    return value ? std::optional(std::get<std::int32_t>(*value)) : std::nullopt;
}

std::optional<float> Registry::GetFloat(Id mod, Id id) const
{
    const auto value = Get(mod, id, Kind::Float);
    return value ? std::optional(std::get<float>(*value)) : std::nullopt;
}

std::optional<std::int32_t> Registry::GetEnum(Id mod, Id id) const
{
    const auto value = Get(mod, id, Kind::Enum);
    return value ? std::optional(std::get<std::int32_t>(*value)) : std::nullopt;
}

std::optional<std::int32_t> Registry::GetKeybind(Id mod, Id id) const
{
    const auto value = Get(mod, id, Kind::Keybind);
    return value ? std::optional(std::get<std::int32_t>(*value)) : std::nullopt;
}

bool Registry::Set(Id mod, Id id, Kind kind, Value value)
{
    std::lock_guard lock(mutex_);
    const auto module = registered_.find(mod);
    if (module == registered_.end())
        return false;
    const auto setting = module->second.find(id);
    if (setting == module->second.end() || setting->second.kind != kind)
        return false;
    setting->second.value = Clamp(setting->second, value);
    return true;
}

bool Registry::SetBool(Id mod, Id id, bool value)
{
    return Set(mod, id, Kind::Boolean, value);
}

bool Registry::SetInt(Id mod, Id id, std::int32_t value)
{
    return Set(mod, id, Kind::Integer, value);
}

bool Registry::SetFloat(Id mod, Id id, float value)
{
    if (!std::isfinite(value))
        return false;
    return Set(mod, id, Kind::Float, value);
}

bool Registry::SetEnum(Id mod, Id id, std::int32_t index)
{
    return Set(mod, id, Kind::Enum, index);
}

bool Registry::SetKeybind(Id mod, Id id, std::int32_t virtualKey)
{
    if (virtualKey < 0 || virtualKey > 255)
        return false;
    return Set(mod, id, Kind::Keybind, virtualKey);
}

bool Registry::Reset(Id mod, Id id)
{
    std::lock_guard lock(mutex_);
    const auto module = registered_.find(mod);
    if (module == registered_.end())
        return false;
    const auto setting = module->second.find(id);
    if (setting == module->second.end())
        return false;
    setting->second.value = setting->second.defaultValue;
    return true;
}
std::vector<ModSnapshot> Registry::Snapshot() const
{
    std::lock_guard lock(mutex_);
    std::vector<ModSnapshot> modules;
    modules.reserve(registered_.size());
    for (const auto& [modId, settings] : registered_)
    {
        ModSnapshot mod{modId, {}};
        mod.settings.reserve(settings.size());
        for (const auto& [id, setting] : settings)
            mod.settings.push_back({id, setting.kind, setting.value, setting.defaultValue,
                                    setting.minimum, setting.maximum, setting.choices});
        modules.push_back(std::move(mod));
    }
    return modules;
}

} // namespace m1core
