#include <stdafx.h>

#include "ScriptStore.h"
#include "ScriptFormats.hpp"

#include <M1CoreRuntime.h>

#include "Utils.h"

ScriptStore::ScriptStore(LuaSandbox& aLuaSandbox, const Paths& aPaths, VKBindings& aBindings)
    : m_sandbox(aLuaSandbox)
    , m_paths(aPaths)
    , m_bindings(aBindings)
{
}

void ScriptStore::DiscardAll()
{
    m_vkBinds.clear();
    m_contexts.clear();
    m_sandbox.ResetState();
    m_bindings.Load();
}

void ScriptStore::LoadAll()
{
    DiscardAll();

    const auto consoleLogger = spdlog::get("scripting");

    std::size_t failedMods = 0;
    std::size_t skippedMods = 0;

    const auto& cModsRoot = m_paths.ModsRoot();
    const auto discovery = m1core::scripts::DiscoverModDirectories(cModsRoot);
    if (discovery.error)
        consoleLogger->error("Could not fully scan m1core mods at '{}': {}",
                             UTF16ToUTF8(cModsRoot.native()), discovery.error.message());

    for (const auto& modDir : discovery.directories)
    {
        const auto name = UTF16ToUTF8(modDir.filename().native());
        const auto pathStr = UTF16ToUTF8(modDir.native());

        if (name == "cet")
        {
            consoleLogger->warn("Ignoring mod using reserved name! ('{}')", pathStr);
            ++skippedMods;
            continue;
        }

        // A broken junction, inaccessible directory or malformed entrypoint
        // should only fail the affected mod, never stop loading other mods.
        try
        {
            const auto path = GetAbsolutePath(modDir, cModsRoot, false);
            if (path.empty())
            {
                consoleLogger->error("Tried to access invalid directory! ('{}')", pathStr);
                ++failedMods;
                continue;
            }

            if (!is_directory(path))
            {
                ++skippedMods;
                continue; // Could be a symlink to a file.
            }

            if (m1core::scripts::FindEntrypoint(path).empty())
            {
                consoleLogger->warn("Ignoring mod without init.m1s, init.lua or init.m1c! ('{}')", pathStr);
                ++skippedMods;
                continue;
            }

            auto ctx = ScriptContext{m_sandbox, path, name};
            if (ctx.IsValid())
            {
                m_contexts.emplace(name, std::move(ctx));
                consoleLogger->info("Mod {} loaded! ('{}')", name, pathStr);
            }
            else
            {
                ++failedMods;
                consoleLogger->error("Mod {} failed to load! ('{}')", name, pathStr);
            }
        }
        catch (const std::exception& error)
        {
            ++failedMods;
            consoleLogger->error("Mod {} could not be loaded: {} ('{}')", name, error.what(), pathStr);
        }
    }

    for (auto& contextIt : m_contexts)
        m_vkBinds.insert({contextIt.first, contextIt.second.GetBinds()});

    m_bindings.InitializeMods(m_vkBinds);
    consoleLogger->info("m1core Lua mod scan: {} loaded, {} failed, {} skipped",
                        m_contexts.size(), failedMods, skippedMods);
}

const VKBind* ScriptStore::GetBind(const VKModBind& acModBind) const
{
    if (acModBind == Bindings::GetOverlayToggleModBind())
        return &Bindings::GetOverlayToggleBind();

    const auto it = m_contexts.find(acModBind.ModName);
    if (it != m_contexts.cend())
        return it->second.GetBind(acModBind.ID);

    return nullptr;
}

const TiltedPhoques::Vector<VKBind>* ScriptStore::GetBinds(const std::string& acModName) const
{
    const auto it = m_contexts.find(acModName);
    if (it != m_contexts.cend())
    {
        return &it->second.GetBinds();
    }

    return nullptr;
}

const TiltedPhoques::Map<std::string, std::reference_wrapper<const TiltedPhoques::Vector<VKBind>>>& ScriptStore::GetAllBinds() const
{
    return m_vkBinds;
}

void ScriptStore::TriggerOnHook() const
{
    for (const auto& mod : m_contexts | std::views::values)
        mod.TriggerOnHook();
}

void ScriptStore::TriggerOnTweak() const
{
    for (const auto& mod : m_contexts | std::views::values)
        mod.TriggerOnTweak();
}

void ScriptStore::TriggerOnInit() const
{
    for (const auto& mod : m_contexts | std::views::values)
        mod.TriggerOnInit();
}

void ScriptStore::TriggerOnUpdate(float aDeltaTime) const
{
    for (const auto& mod : m_contexts | std::views::values)
        mod.TriggerOnUpdate(aDeltaTime);
}

void ScriptStore::TriggerOnDraw() const
{
    for (const auto& mod : m_contexts | std::views::values)
        mod.TriggerOnDraw();
}

void ScriptStore::TriggerOnOverlayOpen() const
{
    for (const auto& mod : m_contexts | std::views::values)
        mod.TriggerOnOverlayOpen();
}

void ScriptStore::TriggerOnOverlayClose() const
{
    for (const auto& mod : m_contexts | std::views::values)
        mod.TriggerOnOverlayClose();
}

sol::object ScriptStore::GetMod(const std::string& acName) const
{
    const auto it = m_contexts.find(acName);
    if (it != m_contexts.cend())
        return it->second.GetRootObject();

    return sol::nil;
}
