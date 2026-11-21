// RED4ext-native loader for the gifted MIT M1CoreRuntime 1.38.1 runtime.
// Copyright (c) 2018 yamashi; Copyright (c) 2026 MISO.
// Original MIT notices: ../LICENSE-MIT and ../../LICENSE.
#include <stdafx.h>

#include "M1CoreRuntime.h"
#include "m1core_registry_api.hpp"
#include "state.hpp"
#include <RED4ext/RED4ext.hpp>

#include <atomic>

namespace {
std::atomic_bool g_initialized{false};
bool g_ownsMinHook = false;

void Report(const RED4ext::v1::Sdk* sdk, RED4ext::v1::PluginHandle handle,
            const char* message, bool warning = false)
{
    if (!sdk || !sdk->logger) return;
    if (warning && sdk->logger->Warn) sdk->logger->Warn(handle, message);
    else if (!warning && sdk->logger->Info) sdk->logger->Info(handle, message);
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(
    RED4ext::v1::PluginHandle handle, RED4ext::v1::EMainReason reason,
    const RED4ext::v1::Sdk* sdk)
{
    if (reason == RED4ext::v1::EMainReason::Unload)
    {
        if (g_initialized.exchange(false))
        {
            // Restore the donated M1CoreRuntime loader's shutdown order: first
            // unhook our bundled MinHook runtime, then destroy M1CoreRuntime objects.
            // The MinHook package is statically linked into this DLL.
            if (g_ownsMinHook) MH_DisableHook(MH_ALL_HOOKS);
            M1CoreRuntime::Shutdown();
        }
        if (g_ownsMinHook)
        {
            MH_Uninitialize();
            g_ownsMinHook = false;
        }
        return true;
    }
    if (reason != RED4ext::v1::EMainReason::Load) return true;

    const MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
    {
        Report(sdk,handle,"m1core: MinHook initialization failed",true);
        return false;
    }
    g_ownsMinHook = (status == MH_OK);

    try
    {
        // Preserve the donated M1CoreRuntime runtime's connected subsystems:
        // Window, VKBindings, D3D12, Overlay, Scripting and GameMainThread.
        M1CoreRuntime::Initialize();
        // Keep the existing m1core native registry and REDscript API.
        // The M1CoreRuntime window/input/render system replaces only its old frontend.
        m1core::Store().Configure(M1CoreRuntime::Get().GetPaths().CoreRoot() / L"settings.json");
        m1core::RegisterNativeModAPI();
        g_initialized.store(true);
        Report(sdk,handle,"m1core: native runtime and settings registry initialized");
        return true;
    }
    catch (...)
    {
        Report(sdk,handle,"m1core: donated runtime initialization failed",true);
        if (g_ownsMinHook) { MH_Uninitialize(); g_ownsMinHook = false; }
        return false;
    }
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info)
{
    info->name = L"m1core";
    info->author = L"MISO";
    info->version = RED4EXT_V1_SEMVER(1,0,0);
    info->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    info->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT std::uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
