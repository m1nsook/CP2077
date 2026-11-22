#include <stdafx.h>

#include "ScriptStore.h"
#include "ScriptFormats.hpp"
#include "ModDispatch.hpp"
#include "config/M1CoreVersion.h"

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
    {
        std::lock_guard lock(m_compatibilityMutex);
        m_compatibilityIssues.clear();
    }
    m_vkBinds.clear();
    m_contexts.clear();
    m_sandbox.ResetState();
    m_bindings.Load();
}

void ScriptStore::LoadAll()
{
    DiscardAll();

    const auto consoleLogger = spdlog::get("scripting");

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
                continue;
            }

            if (!is_directory(path))
                continue; // Could be a symlink to a file.

            if (m1core::scripts::FindEntrypoint(path).empty())
            {
                consoleLogger->warn("Ignoring mod without init.m1s, init.lua or init.m1c! ('{}')", pathStr);
                continue;
            }

            // Reject incompatible mods BEFORE creating a sandbox or running Lua.
            const auto check = m1core::compat::ReadManifest(path, name,
                {M1CORE_VERSION_MAJOR, M1CORE_VERSION_MINOR, M1CORE_VERSION_PATCH});
            if (check.issue)
            {
                {
                    std::lock_guard lock(m_compatibilityMutex);
                    m_compatibilityIssues.push_back(*check.issue);
                }
                consoleLogger->warn("Skipping mod '{}' (version '{}'): {} (installed m1core {})",
                    check.issue->name, check.issue->modVersion,
                    check.issue->detail, check.issue->installedCore);
                continue;
            }

            auto ctx = ScriptContext{m_sandbox, path, name};
            if (ctx.IsValid())
            {
                m_contexts.emplace(name, std::move(ctx));
                consoleLogger->info("Mod {} loaded! ('{}')", name, pathStr);
            }
            else
                consoleLogger->error("Mod {} failed to load! ('{}')", name, pathStr);
        }
        catch (const std::exception& error)
        {
            consoleLogger->error("Mod {} could not be loaded: {} ('{}')", name, error.what(), pathStr);
        }
        catch (...)
        {
            consoleLogger->error("Mod {} could not be loaded: unknown native C++ exception ('{}')", name, pathStr);
        }
    }

    for (auto& contextIt : m_contexts)
        m_vkBinds.insert({contextIt.first, contextIt.second.GetBinds()});

    m_bindings.InitializeMods(m_vkBinds);
}

std::vector<m1core::compat::Issue> ScriptStore::GetCompatibilityIssues() const
{
    std::lock_guard lock(m_compatibilityMutex);
    return m_compatibilityIssues;
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

namespace
{
void ReportModLifecycleFailure(std::string_view mod, std::string_view event,
                               const char* reason)
{
    if (const auto logger = spdlog::get("scripting"))
        logger->error("Mod '{}' failed during {}: {}", mod, event, reason);
}
} // namespace

void ScriptStore::TriggerOnHook() const
{
    m1core::scripts::ForEachModSafely(m_contexts, "onHook",
        [&](const ScriptContext& mod) { mod.TriggerOnHook(); },
        ReportModLifecycleFailure);
}

void ScriptStore::TriggerOnTweak() const
{
    m1core::scripts::ForEachModSafely(m_contexts, "onTweak",
        [&](const ScriptContext& mod) { mod.TriggerOnTweak(); },
        ReportModLifecycleFailure);
}

void ScriptStore::TriggerOnInit() const
{
    m1core::scripts::ForEachModSafely(m_contexts, "onInit",
        [&](const ScriptContext& mod) { mod.TriggerOnInit(); },
        ReportModLifecycleFailure);
}

void ScriptStore::TriggerOnUpdate(float aDeltaTime) const
{
    m1core::scripts::ForEachModSafely(m_contexts, "onUpdate",
        [&](const ScriptContext& mod) { mod.TriggerOnUpdate(aDeltaTime); },
        ReportModLifecycleFailure);
}

void ScriptStore::TriggerOnDraw() const
{
    m1core::scripts::ForEachModSafely(m_contexts, "onDraw",
        [&](const ScriptContext& mod) { mod.TriggerOnDraw(); },
        ReportModLifecycleFailure);
}

void ScriptStore::TriggerOnOverlayOpen() const
{
    m1core::scripts::ForEachModSafely(m_contexts, "onOverlayOpen",
        [&](const ScriptContext& mod) { mod.TriggerOnOverlayOpen(); },
        ReportModLifecycleFailure);
}

void ScriptStore::TriggerOnOverlayClose() const
{
    m1core::scripts::ForEachModSafely(m_contexts, "onOverlayClose",
        [&](const ScriptContext& mod) { mod.TriggerOnOverlayClose(); },
        ReportModLifecycleFailure);
}


sol::object ScriptStore::GetMod(const std::string& acName) const
{
    const auto it = m_contexts.find(acName);
    if (it != m_contexts.cend())
        return it->second.GetRootObject();

    return sol::nil;
}
