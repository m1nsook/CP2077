#include <stdafx.h>

#include "Overlay.h"

#include <M1CoreRuntime.h>

#include <d3d12/D3D12.h>
#include <scripting/LuaVM.h>
#include <Utils.h>
#include "m1core_registry_ui.hpp"
#include "PhosphorIcons.h"

void Overlay::PostInitialize()
{
    if (!m_initialized)
    {
        if (Bindings::IsFirstTimeSetup())
        {
            Toggle();

            auto& d3d12 = M1CoreRuntime::Get().GetD3D12();
            d3d12.DelayedSetTrapInputInImGui(true);
        }
        else
        {
            const auto cOverlayBindCode = M1CoreRuntime::Get().GetBindings().GetBindCodeForModBind(Bindings::GetOverlayToggleModBind());
            Toast startup{ToastType::Info, NOTIFY_DEFAULT_DISMISS,
                          "Shortcut: %s", VKBindings::GetBindString(cOverlayBindCode).c_str()};
            startup.setTitle("m1core");
            startup.setIcon(M1CORE_ICON_ABOUT);
            startup.setPosition(ImNotifyToastPos::BottomLeft);
            ImGui::InsertNotification(startup);
        }

        m_initialized = true;
    }
}

Console& Overlay::GetConsole()
{
    return m_console;
}

Bindings& Overlay::GetBindings()
{
    return m_bindings;
}

Settings& Overlay::GetSettings()
{
    return m_settings;
}

void Overlay::Toggle()
{
    if (!m_toggled)
        m_toggled = true;
}

bool Overlay::IsEnabled() const noexcept
{
    // return true also when overlay is closed but toggling on/off
    return m_initialized && (m_enabled || m_toggled);
}

void Overlay::Update()
{
    if (!m_initialized) return;

    // Keep the proven M1CoreRuntime 1.38.1 bind/setup, overlay toggle and cursor gate.
    // Replace only its user-facing toolbar/console/editor/debug panels.
    if (m_toggled) {
        if (m_bindings.FirstTimeSetup())
            return;
        if (m_enabled) m_vm.OnOverlayClose();
        else m_vm.OnOverlayOpen();
        m_enabled = !m_enabled;
        M1CoreRuntime::Get().GetD3D12().DelayedSetTrapInputInImGui(m_enabled);
        m_toggled = false;
    }

    ImGui::RenderNotifications();
    if (!m_enabled) return;

    m1core::DrawNativeRegistryUI();
    // Used only when the user explicitly opens shortcut editing. The donor
    // widget remains responsible for capturing and persisting key combos.
    m_bindings.Draw();
}

bool Overlay::IsInitialized() const noexcept
{
    return m_initialized;
}

Overlay::Overlay(VKBindings& aBindings, Options& aOptions, PersistentState& aPersistentState, LuaVM& aVm)
    : m_console(aOptions, aPersistentState, aVm)
    , m_bindings(aBindings, aVm)
    , m_settings(aOptions, aVm)
    , m_tweakDBEditor(aVm)
    , m_options(aOptions)
    , m_persistentState(aPersistentState)
    , m_vm(aVm)
{
    GameMainThread::Get().AddBaseInitializationTask(
        [this]
        {
            PostInitialize();
            return true;
        });
}

Overlay::~Overlay()
{
}

