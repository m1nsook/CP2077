#pragma once

#include <nlohmann/json.hpp>
#include <array>
#include <charconv>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

// A mod's own version is independent of its m1core runtime requirement.
// The absent mod.json case intentionally preserves existing legacy mods.
namespace m1core::compat
{
struct Version
{
    std::uint32_t major{}, minor{}, patch{};
    auto operator<=>(const Version&) const = default;
};

inline std::optional<Version> ParseVersion(std::string_view value)
{
    std::array<std::uint32_t, 3> numbers{};
    for (std::size_t index = 0; index < numbers.size(); ++index)
    {
        const auto dot = value.find('.');
        const auto segment = value.substr(0, dot);
        if (segment.empty() || (segment.size() > 1 && segment.front() == '0'))
            return std::nullopt;
        const auto parsed = std::from_chars(segment.data(), segment.data() + segment.size(), numbers[index]);
        if (parsed.ec != std::errc{} || parsed.ptr != segment.data() + segment.size())
            return std::nullopt;
        if (index != 2)
        {
            if (dot == std::string_view::npos) return std::nullopt;
            value.remove_prefix(dot + 1);
        }
        else if (dot != std::string_view::npos)
            return std::nullopt;
    }
    return Version{numbers[0], numbers[1], numbers[2]};
}

inline std::string ToString(Version value)
{
    return std::to_string(value.major) + "." + std::to_string(value.minor) + "." + std::to_string(value.patch);
}

enum class Reason { NeedsNewerCore, NeedsOlderCore, InvalidManifest };

struct Issue
{
    std::string folder;
    std::string name;
    std::string modVersion;
    std::string installedCore;
    std::string requiredMin;
    std::string maximumExclusive;
    std::string detail;
    Reason reason{Reason::InvalidManifest};
};

struct Check
{
    std::optional<Issue> issue; // Empty means the mod is allowed to load.
};

inline Check Invalid(std::string_view folder, Version installed, std::string detail)
{
    if (detail.size() > 240) detail.resize(240);
    Issue issue;
    issue.folder = folder;
    issue.name = folder;
    issue.installedCore = ToString(installed);
    issue.detail = std::move(detail);
    return {std::move(issue)};
}

// Never execute Lua to determine compatibility; read only mod.json.
inline Check CheckJson(const nlohmann::json& doc, std::string_view folder, Version installed)
{
    if (!doc.is_object())
        return Invalid(folder, installed, "mod.json must be an object.");

    Issue issue;
    issue.folder = folder;
    issue.name = folder;
    issue.installedCore = ToString(installed);
    try
    {
        if (doc.contains("name"))
        {
            if (!doc["name"].is_string())
                return Invalid(folder, installed, "name must be a string.");
            issue.name = doc["name"].get<std::string>();
            if (issue.name.empty() || issue.name.size() > 128)
                return Invalid(folder, installed, "name must be 1 to 128 characters.");
        }
        if (!doc.contains("version") || !doc["version"].is_string())
            return Invalid(folder, installed, "version must be MAJOR.MINOR.PATCH.");
        issue.modVersion = doc["version"].get<std::string>();
        if (!ParseVersion(issue.modVersion))
            return Invalid(folder, installed, "mod version must be MAJOR.MINOR.PATCH.");

        if (!doc.contains("m1core") || !doc["m1core"].is_object())
            return Invalid(folder, installed, "m1core must be an object with a min version.");
        const auto& required = doc["m1core"];
        if (!required.contains("min") || !required["min"].is_string())
            return Invalid(folder, installed, "m1core.min must be a version string.");
        issue.requiredMin = required["min"].get<std::string>();
        const auto min = ParseVersion(issue.requiredMin);
        if (!min)
            return Invalid(folder, installed, "Invalid m1core.min; use MAJOR.MINOR.PATCH.");

        std::optional<Version> max;
        if (required.contains("max_exclusive"))
        {
            if (!required["max_exclusive"].is_string())
                return Invalid(folder, installed, "m1core.max_exclusive must be a version string.");
            issue.maximumExclusive = required["max_exclusive"].get<std::string>();
            max = ParseVersion(issue.maximumExclusive);
            if (!max || *max <= *min)
                return Invalid(folder, installed, "max_exclusive must be greater than min.");
        }
        if (installed < *min)
        {
            issue.reason = Reason::NeedsNewerCore;
            issue.detail = "Requires m1core " + issue.requiredMin + " or newer.";
            return {std::move(issue)};
        }
        if (max && installed >= *max)
        {
            issue.reason = Reason::NeedsOlderCore;
            issue.detail = "Requires m1core below " + issue.maximumExclusive + ".";
            return {std::move(issue)};
        }
    }
    catch (const std::exception& error)
    {
        return Invalid(folder, installed, std::string("Invalid mod.json: ") + error.what());
    }
    return {};
}

inline Check ReadManifest(const std::filesystem::path& folderPath, std::string_view folder, Version installed)
{
    const auto path = folderPath / "mod.json";
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) return Invalid(folder, installed, "Cannot access mod.json: " + ec.message());
    if (!exists) return {}; // No forced migration for legacy mods.
    if (!std::filesystem::is_regular_file(path, ec) || ec)
        return Invalid(folder, installed, "mod.json is not a regular file.");
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > 65536)
        return Invalid(folder, installed, "mod.json unreadable or exceeds 64 KiB.");
    std::ifstream input(path, std::ios::binary);
    if (!input) return Invalid(folder, installed, "Cannot open mod.json.");
    try { return CheckJson(nlohmann::json::parse(input), folder, installed); }
    catch (const std::exception& error)
    {
        return Invalid(folder, installed, std::string("Cannot parse mod.json: ") + error.what());
    }
}
} // namespace m1core::compat
