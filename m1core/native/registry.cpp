#include "registry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace m1core {
namespace {
using json = nlohmann::json;

std::optional<SettingKind> ParseKind(const std::string& name)
{
    if (name == "boolean") return SettingKind::Boolean;
    if (name == "integer") return SettingKind::Integer;
    if (name == "float") return SettingKind::Float;
    if (name == "choice") return SettingKind::Choice;
    return std::nullopt;
}

json ReadSaved(const StateStore& store)
{
    try { return json::parse(store.Snapshot()); }
    catch (...) { return json::object(); }
}
}

Registry& Settings()
{
    static Registry registry(Store());
    return registry;
}

bool Registry::ValidId(std::string_view value)
{
    if (value.empty() || value.size() > 96) return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    });
}

std::optional<nlohmann::json> Registry::Normalize(
    const SettingView& setting, const nlohmann::json& input)
{
    switch (setting.kind) {
    case SettingKind::Boolean:
        if (!input.is_boolean()) return std::nullopt;
        return input;
    case SettingKind::Integer:
        if (!input.is_number_integer() && !input.is_number_unsigned())
            return std::nullopt;
        {
            const auto value = input.get<double>();
            if (!std::isfinite(value)) return std::nullopt;
            const double clamped = std::clamp(value, setting.minimum, setting.maximum);
            if (clamped < static_cast<double>(std::numeric_limits<std::int32_t>::min()) ||
                clamped > static_cast<double>(std::numeric_limits<std::int32_t>::max()))
                return std::nullopt;
            return static_cast<std::int32_t>(clamped);
        }
    case SettingKind::Float:
        if (!input.is_number()) return std::nullopt;
        {
            const double value = input.get<double>();
            if (!std::isfinite(value)) return std::nullopt;
            return std::clamp(value, setting.minimum, setting.maximum);
        }
    case SettingKind::Choice:
        if (!input.is_string()) return std::nullopt;
        {
            const auto value = input.get<std::string>();
            if (std::find(setting.choices.begin(), setting.choices.end(), value) ==
                setting.choices.end()) return std::nullopt;
            return value;
        }
    }
    return std::nullopt;
}

bool Registry::Register(std::string id, std::string title, const nlohmann::json& definitions)
{
    if (!ValidId(id) || title.empty() || title.size() > 150 ||
        !definitions.is_array() || definitions.empty() || definitions.size() > 250)
        return false;

    ModView prepared{.id = id, .title = std::move(title)};
    const json saved = ReadSaved(store_);
    for (const auto& item : definitions) {
        if (!item.is_object()) return false;
        if (!item.contains("id") || !item["id"].is_string() ||
            !item.contains("kind") || !item["kind"].is_string() ||
            !item.contains("default")) return false;

        SettingView row;
        row.id = item["id"].get<std::string>();
        if (!ValidId(row.id)) return false;
        if (std::any_of(prepared.settings.begin(), prepared.settings.end(),
                [&](const SettingView& old) { return old.id == row.id; }))
            return false;

        const auto kind = ParseKind(item["kind"].get<std::string>());
        if (!kind) return false;
        row.kind = *kind;
        row.label = item.value("label", row.id);
        row.description = item.value("description", std::string{});
        row.group = item.value("group", std::string("General"));
        if (row.label.size() > 140 || row.description.size() > 500 ||
            row.group.size() > 96) return false;

        if (row.kind == SettingKind::Integer || row.kind == SettingKind::Float) {
            if (!item.value("min", json(0)).is_number() ||
                !item.value("max", json(100)).is_number()) return false;
            row.minimum = item.value("min", 0.0);
            row.maximum = item.value("max", 100.0);
            if (!std::isfinite(row.minimum) || !std::isfinite(row.maximum) ||
                row.minimum > row.maximum) return false;
            row.step = item.value("step", 1.0);
            if (!std::isfinite(row.step) || row.step <= 0.0) return false;
        }
        if (row.kind == SettingKind::Choice) {
            if (!item.contains("choices") || !item["choices"].is_array() ||
                item["choices"].empty() || item["choices"].size() > 100) return false;
            for (const auto& option : item["choices"]) {
                if (!option.is_string()) return false;
                auto word = option.get<std::string>();
                if (word.empty() || word.size() > 100 ||
                    std::find(row.choices.begin(), row.choices.end(), word) != row.choices.end())
                    return false;
                row.choices.push_back(std::move(word));
            }
        }

        const auto def = Normalize(row, item["default"]);
        if (!def) return false;
        row.defaultValue = *def;
        row.value = row.defaultValue;

        if (saved.contains("values") && saved["values"].is_object() &&
            saved["values"].contains(id) && saved["values"][id].is_object() &&
            saved["values"][id].contains(row.id)) {
            const auto restored = Normalize(row, saved["values"][id][row.id]);
            if (restored) row.value = *restored;
        }
        prepared.settings.push_back(std::move(row));
    }

    std::lock_guard guard(mutex_);
    mods_[id] = std::move(prepared);
    return true;
}

std::vector<ModView> Registry::Snapshot() const
{
    std::lock_guard guard(mutex_);
    std::vector<ModView> result;
    result.reserve(mods_.size());
    for (const auto& [id, mod] : mods_) result.push_back(mod);
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        return a.title < b.title;
    });
    return result;
}

std::optional<nlohmann::json> Registry::Get(
    std::string_view mod, std::string_view key) const
{
    std::lock_guard guard(mutex_);
    const auto it = mods_.find(std::string(mod));
    if (it == mods_.end()) return std::nullopt;
    const auto found = std::find_if(it->second.settings.begin(), it->second.settings.end(),
        [&](const SettingView& s) { return s.id == key; });
    if (found == it->second.settings.end()) return std::nullopt;
    return found->value;
}

bool Registry::Persist(const std::function<void(json&)>& update)
{
    if (!store_.Update(update)) {
        error_ = store_.LastError();
        if (error_.empty()) error_ = "Failed to persist settings";
        return false;
    }
    error_.clear();
    return true;
}

bool Registry::Commit(std::string_view mod, std::string_view key, const json& next)
{
    const auto modId = std::string(mod);
    const auto settingId = std::string(key);
    return Persist([&](json& data) {
        auto& values = data["values"];
        if (!values.contains(modId) || !values[modId].is_object())
            values[modId] = json::object();
        values[modId][settingId] = next;
    });
}

bool Registry::Set(std::string_view mod, std::string_view key, const json& input)
{
    std::lock_guard guard(mutex_);
    const auto it = mods_.find(std::string(mod));
    if (it == mods_.end()) {
        error_ = "Unknown mod";
        return false;
    }
    auto found = std::find_if(it->second.settings.begin(), it->second.settings.end(),
        [&](const SettingView& s) { return s.id == key; });
    if (found == it->second.settings.end()) {
        error_ = "Unknown setting";
        return false;
    }
    const auto next = Normalize(*found, input);
    if (!next) {
        error_ = "Invalid setting value";
        return false;
    }
    if (*next == found->value) {
        error_.clear();
        return true;
    }
    if (!Commit(mod, key, *next)) return false;
    found->value = *next;
    return true;
}

bool Registry::Reset(std::string_view mod, std::string_view key)
{
    const auto snapshot = Snapshot();
    for (const auto& candidate : snapshot) if (candidate.id == mod)
        for (const auto& setting : candidate.settings) if (setting.id == key)
            return Set(mod, key, setting.defaultValue);
    std::lock_guard guard(mutex_);
    error_ = "Unknown mod or setting";
    return false;
}

bool Registry::ResetMod(std::string_view mod)
{
    std::lock_guard guard(mutex_);
    const auto it = mods_.find(std::string(mod));
    if (it == mods_.end()) {
        error_ = "Unknown mod";
        return false;
    }
    if (!Persist([&](json& document) {
        auto& values = document["values"];
        values[it->first] = json::object();
        for (const auto& setting : it->second.settings)
            values[it->first][setting.id] = setting.defaultValue;
    })) return false;
    for (auto& setting : it->second.settings) setting.value = setting.defaultValue;
    return true;
}

bool Registry::SaveProfile(std::string name)
{
    std::lock_guard guard(mutex_);
    if (!ValidId(name)) {
        error_ = "Invalid profile name";
        return false;
    }
    return Persist([&](json& document) {
        // Preserve values for mods that are not currently registered.
        auto profile = document["values"];
        for (const auto& [id, mod] : mods_) {
            if (!profile.contains(id) || !profile[id].is_object())
                profile[id] = json::object();
            for (const auto& setting : mod.settings)
                profile[id][setting.id] = setting.value;
        }
        document["profiles"][name] = std::move(profile);
    });
}

bool Registry::LoadProfile(std::string_view name)
{
    std::lock_guard guard(mutex_);
    if (!ValidId(name)) {
        error_ = "Invalid profile name";
        return false;
    }
    struct Change { std::string mod, key; json value; };
    std::vector<Change> updates;
    bool valid = true;
    std::string invalidReason;
    const bool saved = Persist([&](json& document) {
        const auto profileIt = document["profiles"].find(std::string(name));
        if (profileIt == document["profiles"].end() || !profileIt->is_object()) {
            valid = false;
            invalidReason = "Profile not found or malformed";
            return;
        }

        // Validate every registered setting BEFORE touching the document.
        // Unregistered mods are preserved for registration after a reload.
        const json profile = *profileIt;
        for (auto it = profile.begin(); it != profile.end(); ++it) {
            const auto& modId = it.key();
            const auto& record = it.value();
            if (!record.is_object()) {
                valid = false;
                invalidReason = "Malformed profile mod record";
                return;
            }
            const auto registered = mods_.find(modId);
            if (registered == mods_.end()) continue;
            for (const auto& setting : registered->second.settings) {
                if (!record.contains(setting.id)) continue;
                const auto next = Normalize(setting, record[setting.id]);
                if (!next) {
                    valid = false;
                    invalidReason = "Invalid profile setting value";
                    return;
                }
                updates.push_back({modId, setting.id, *next});
            }
        }

        // Preserve newly introduced settings and offline-mod values.
        for (auto it = profile.begin(); it != profile.end(); ++it) {
            const auto& modId = it.key();
            if (!document["values"].contains(modId) ||
                !document["values"][modId].is_object())
                document["values"][modId] = json::object();
            for (auto setting = it.value().begin(); setting != it.value().end(); ++setting)
                document["values"][modId][setting.key()] = setting.value();
        }
        // Apply the SAME normalized values to disk as to the live registry:
        // otherwise an out-of-range profile could leave JSON and UI divergent.
        for (const auto& change : updates)
            document["values"][change.mod][change.key] = change.value;
    });
    if (!saved) return false;
    if (!valid) {
        error_ = std::move(invalidReason);
        return false;
    }
    for (const auto& change : updates)
        for (auto& setting : mods_.at(change.mod).settings)
            if (setting.id == change.key) setting.value = change.value;
    return true;
}

bool Registry::DeleteProfile(std::string_view name)
{
    std::lock_guard guard(mutex_);
    if (!ValidId(name)) {
        error_ = "Invalid profile name";
        return false;
    }
    bool found = false;
    const bool saved = Persist([&](json& document) {
        found = document["profiles"].erase(std::string(name)) != 0;
    });
    if (!saved) return false;
    if (!found) {
        error_ = "Profile not found";
        return false;
    }
    return true;
}

std::vector<std::string> Registry::Profiles() const
{
    auto document = ReadSaved(store_);
    std::vector<std::string> result;
    if (!document.contains("profiles") || !document["profiles"].is_object()) return result;
    for (auto it = document["profiles"].begin(); it != document["profiles"].end(); ++it)
        result.push_back(it.key());
    std::sort(result.begin(), result.end());
    return result;
}

std::string Registry::LastError() const
{
    std::lock_guard guard(mutex_);
    return error_;
}
} // namespace m1core
