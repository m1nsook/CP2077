#include <stdafx.h>

const std::filesystem::path& Paths::Executable() const
{
    return m_exe;
}

const std::filesystem::path& Paths::GameRoot() const
{
    return m_gameRoot;
}

const std::filesystem::path& Paths::CoreRoot() const
{
    return m_coreRoot;
}

const std::filesystem::path& Paths::Config() const
{
    return m_config;
}

const std::filesystem::path& Paths::PersistentState() const
{
    return m_persistentState;
}

const std::filesystem::path& Paths::VKBindings() const
{
    return m_vkBindings;
}

const std::filesystem::path& Paths::ModsRoot() const
{
    return m_modsRoot;
}

const std::filesystem::path& Paths::R6CacheModdedRoot() const
{
    return m_r6CacheModdedRoot;
}

const std::filesystem::path& Paths::Fonts() const
{
    return m_fonts;
}

const std::filesystem::path& Paths::TweakDB() const
{
    return m_tweakdb;
}

Paths::Paths()
{
    TCHAR exePathBuf[MAX_PATH] = {0};
    GetModuleFileName(GetModuleHandle(nullptr), exePathBuf, static_cast<DWORD>(std::size(exePathBuf)));
    m_exe = exePathBuf;

    m_gameRoot = m_exe.parent_path();

    // From bin/x64/Cyberpunk2077.exe, use the native RED4ext
    // plugin directory. Never read/write an existing CET installation.
    m_coreRoot = (m_gameRoot / L".." / L".." /
                 L"red4ext" / L"plugins" / L"m1core").lexically_normal();
    create_directories(m_coreRoot);

    m_config = m_coreRoot / L"config.json";
    // remove empty config.json
    if (exists(m_config) && !file_size(m_config))
        std::filesystem::remove(m_config);

    m_persistentState = m_coreRoot / L"persistent.json";
    // remove empty persistentState.json
    if (exists(m_persistentState) && !file_size(m_persistentState))
        std::filesystem::remove(m_persistentState);

    m_vkBindings = m_coreRoot / L"bindings.json";
    // remove empty vkbindings.json
    if (exists(m_vkBindings) && !file_size(m_vkBindings))
        std::filesystem::remove(m_vkBindings);

    m_modsRoot = m_coreRoot / L"mods";
    create_directories(m_modsRoot);
    // The author package has no script resources or icon scripts.
    create_directories(m_coreRoot / L"scripts");

    m_r6CacheModdedRoot = m_gameRoot;
    m_r6CacheModdedRoot /= L"..";
    m_r6CacheModdedRoot /= L"..";
    m_r6CacheModdedRoot /= L"r6";
    m_r6CacheModdedRoot /= L"cache";
    m_r6CacheModdedRoot /= L"modded";

    m_fonts = m_coreRoot / L"fonts";

    m_tweakdb = m_coreRoot / L"tweakdb";
}
