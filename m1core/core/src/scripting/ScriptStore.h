#pragma once

#include "ScriptContext.h"
#include "ModCompatibility.hpp"
#include <mutex>
#include <vector>

struct ScriptStore
{
    ScriptStore(LuaSandbox& aLuaSandbox, const Paths& aPaths, VKBindings& aBindings);
    ~ScriptStore() = default;

    void DiscardAll();
    void LoadAll();

    [[nodiscard]] const VKBind* GetBind(const VKModBind& acModBind) const;
    [[nodiscard]] const TiltedPhoques::Vector<VKBind>* GetBinds(const std::string& acModName) const;
    [[nodiscard]] const TiltedPhoques::Map<std::string, std::reference_wrapper<const TiltedPhoques::Vector<VKBind>>>& GetAllBinds() const;

    void TriggerOnHook() const;
    void TriggerOnTweak() const;
    void TriggerOnInit() const;
    void TriggerOnUpdate(float aDeltaTime) const;
    void TriggerOnDraw() const;

    void TriggerOnOverlayOpen() const;
    void TriggerOnOverlayClose() const;

    [[nodiscard]] sol::object GetMod(const std::string& acName) const;
    [[nodiscard]] std::vector<m1core::compat::Issue> GetCompatibilityIssues() const;

private:
    TiltedPhoques::Map<std::string, ScriptContext> m_contexts{};
    mutable std::mutex m_compatibilityMutex{};
    std::vector<m1core::compat::Issue> m_compatibilityIssues{};
    TiltedPhoques::Map<std::string, std::reference_wrapper<const TiltedPhoques::Vector<VKBind>>> m_vkBinds{};
    LuaSandbox& m_sandbox;
    const Paths& m_paths;
    VKBindings& m_bindings;
};