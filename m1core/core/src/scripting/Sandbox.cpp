#include <stdafx.h>

#include "Sandbox.h"
#include "Scripting.h"
#include "ScriptFormats.hpp"

Sandbox::Sandbox(uint64_t aId, Scripting* apScripting, sol::table aBaseEnvironment, const std::filesystem::path& acRootPath)
    : m_id(aId)
    , m_pScripting(apScripting)
    , m_env(apScripting->GetLockedState().Get(), sol::create, aBaseEnvironment)
    , m_path(acRootPath)
{
}

sol::protected_function_result Sandbox::ExecuteFile(const std::string& acPath) const
{
    const auto kind = m1core::scripts::Classify(acPath);
    if (kind == m1core::scripts::Format::LuaJitBytecode)
    {
        // Only an explicitly named .m1c file can enable binary loading.
        // LuaJIT bytecode is version-specific and untrusted bytecode may
        // bypass Lua sandbox guarantees. Mod packages are executable code.
        std::ifstream input(acPath, std::ios::binary);
        char signature[4]{};
        input.read(signature, sizeof(signature));
        if (!m1core::scripts::IsLuaJitBytecode(
                std::string_view(signature, static_cast<size_t>(input.gcount()))))
            throw std::runtime_error("Invalid .m1c: expected LuaJIT bytecode");
    }

    return m_pScripting->GetLockedState().Get().script_file(
        acPath, m_env,
        kind == m1core::scripts::Format::LuaJitBytecode
            ? sol::load_mode::binary : sol::load_mode::text);
}

sol::protected_function_result Sandbox::ExecuteString(const std::string& acString) const
{
    return m_pScripting->GetLockedState().Get().script(acString, m_env, sol::detail::default_chunk_name(), sol::load_mode::text);
}

uint64_t Sandbox::GetId() const
{
    return m_id;
}

sol::environment& Sandbox::GetEnvironment()
{
    return m_env;
}

const std::filesystem::path& Sandbox::GetRootPath() const
{
    return m_path;
}
