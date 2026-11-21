#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace m1core::scripts {

// .m1s is Lua source, .m1c is LuaJIT bytecode, .lua remains compatible.
// A name without an extension resolves source before compiled bytecode.
enum class Format { Source, LuaJitBytecode, Other };

inline Format Classify(const std::filesystem::path& file)
{
    auto ext = file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (ext == ".m1s" || ext == ".lua") return Format::Source;
    if (ext == ".m1c") return Format::LuaJitBytecode;
    return Format::Other;
}

// The LuaJIT bytecode magic is ESC L J (not standard Lua's ESC L u a).
// An incompatible LuaJIT version is rejected by the VM in binary-only mode.
inline bool IsLuaJitBytecode(std::string_view bytes) noexcept
{
    return bytes.size() >= 4 && bytes[0] == '\x1b' &&
           bytes[1] == 'L' && bytes[2] == 'J';
}

inline std::filesystem::path FindEntrypoint(const std::filesystem::path& root)
{
    for (const wchar_t* name : {L"init.m1s", L"init.lua", L"init.m1c"})
    {
        auto candidate = root / name;
        if (std::filesystem::is_regular_file(candidate))
            return candidate;
    }
    return {};
}

// Only known extensions count as an explicit format. An extensionless
// require/loadfile name searches m1s, lua and m1c, in that order. Preserve
// CET's historic bare-file lookup first for older modules.
inline std::vector<std::string> Candidates(std::string_view name, bool module)
{
    if (name.empty()) return {};

    std::vector<std::string> result{std::string(name)};
    const std::filesystem::path file{std::string(name)};
    if (Classify(file) != Format::Other) return result;

    for (auto ext : {".m1s", ".lua", ".m1c"})
        result.emplace_back(std::string(name) + ext);

    if (module)
    {
        for (auto ext : {"init.m1s", "init.lua", "init.m1c"})
            result.emplace_back((file / ext).generic_string());
    }
    return result;
}
} // namespace m1core::scripts
