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
    if (type == "int" && value.is_number_integer())
    {
        const auto number = value.get<std::int64_t>();
        if (number >= std::numeric_limits<std::int32_t>::min() &&
            number <= std::numeric_limits<std::int32_t>::max())
            return std::pair{Kind::Integer, Value(static_cast<std::int32_t>(number))};
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

        std::filesystem::create_directories(file_.parent_path());
        auto temporary = file_;
        temporary += ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output)
                return false;
            output << doc.dump(2) << '\n';
            output.flush();
            if (!output)
                return false;
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), file_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
#else
        std::filesystem::rename(temporary, file_);
#endif
        return true;
    }
    catch (...)
    {
        return false;
    }
}

Value Registry::Clamp(const Setting& setting, Value value)
{
    if (setting.kind == Kind::Integer)
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
} // namespace m1core
