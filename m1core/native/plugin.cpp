// M1CORE standalone native RED4ext plugin.
// Independent MIT implementation: no Cyber Engine Tweaks code or runtime.
#include "registry.hpp"
#include "render_bridge.hpp"
#include "state.hpp"
#include "game_lifecycle.hpp"

#include <RED4ext/Api/v1/GameState.hpp>
#include <RED4ext/GameStates.hpp>

#include <RED4ext/RED4ext.hpp>
#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>

namespace m1core {
namespace {
std::atomic_bool initializedBase{false};
}
bool GameBaseInitialized() noexcept
{
    return initializedBase.load(std::memory_order_acquire);
}
void SetGameBaseInitialized() noexcept
{
    initializedBase.store(true, std::memory_order_release);
}
}

namespace {
using RED4ext::CString;
using RED4ext::CStackFrame;
using RED4ext::IScriptable;

void Version(IScriptable*,CStackFrame* frame,CString* output,std::int64_t)
{
    frame->code++;
    if (output) *output=CString("1.0.0-dev.3");
}
void Register(IScriptable*,CStackFrame* frame,bool* output,std::int64_t)
{
    CString id,title,definitions;
    RED4ext::GetParameter(frame,&id);
    RED4ext::GetParameter(frame,&title);
    RED4ext::GetParameter(frame,&definitions);
    frame->code++;
    bool ok=false;
    try {
        const auto schema=nlohmann::json::parse(definitions.c_str());
        ok=m1core::Settings().Register(id.c_str(),title.c_str(),schema);
    } catch (...) {}
    if (output) *output=ok;
}
void GetValue(IScriptable*,CStackFrame* frame,CString* output,std::int64_t)
{
    CString mod,key;
    RED4ext::GetParameter(frame,&mod);
    RED4ext::GetParameter(frame,&key);
    frame->code++;
    if (output) {
        const auto found=m1core::Settings().Get(mod.c_str(),key.c_str());
        const auto value=found ? found->dump() : "null";
        *output=CString(value.c_str());
    }
}
void SetValue(IScriptable*,CStackFrame* frame,bool* output,std::int64_t)
{
    CString mod,key,value;
    RED4ext::GetParameter(frame,&mod);
    RED4ext::GetParameter(frame,&key);
    RED4ext::GetParameter(frame,&value);
    frame->code++;
    bool ok=false;
    try {
        ok=m1core::Settings().Set(mod.c_str(),key.c_str(),
                                 nlohmann::json::parse(value.c_str()));
    } catch (...) {}
    if (output) *output=ok;
}
void ResetValue(IScriptable*,CStackFrame* frame,bool* output,std::int64_t)
{
    CString mod,key;
    RED4ext::GetParameter(frame,&mod);
    RED4ext::GetParameter(frame,&key);
    frame->code++;
    if (output) *output=m1core::Settings().Reset(mod.c_str(),key.c_str());
}
void ResetMod(IScriptable*,CStackFrame* frame,bool* output,std::int64_t)
{
    CString mod;
    RED4ext::GetParameter(frame,&mod);
    frame->code++;
    if (output) *output=m1core::Settings().ResetMod(mod.c_str());
}
void SaveProfile(IScriptable*,CStackFrame* frame,bool* output,std::int64_t)
{
    CString name;
    RED4ext::GetParameter(frame,&name);
    frame->code++;
    if (output) *output=m1core::Settings().SaveProfile(name.c_str());
}
void LoadProfile(IScriptable*,CStackFrame* frame,bool* output,std::int64_t)
{
    CString name;
    RED4ext::GetParameter(frame,&name);
    frame->code++;
    if (output) *output=m1core::Settings().LoadProfile(name.c_str());
}
void DeleteProfile(IScriptable*,CStackFrame* frame,bool* output,std::int64_t)
{
    CString name;
    RED4ext::GetParameter(frame,&name);
    frame->code++;
    if (output) *output=m1core::Settings().DeleteProfile(name.c_str());
}

bool GameStateContinue(RED4ext::CGameApplication*) { return true; }
bool BaseInitializationFinished(RED4ext::CGameApplication*)
{
    m1core::SetGameBaseInitialized();
    m1core::RenderBridge::Get().LogInfo(
        "m1core: base initialization finished; initial setup may open");
    return true;
}
RED4ext::v1::GameState baseGameCallbacks{
    &GameStateContinue, &GameStateContinue, &BaseInitializationFinished
};

void RegisterTypes() {}
void PostRegisterTypes()
{
    auto* rtti=RED4ext::CRTTISystem::Get();
    if (!rtti) return;
    const auto add=[&](const char* name,
                       RED4ext::ScriptingFunction_t<void*> callback,
                       const char* type,const char* const* args,
                       std::size_t count) {
        auto* fn=RED4ext::CGlobalFunction::Create(name,name,callback);
        fn->flags=RED4ext::CBaseFunction::Flags{
            .isNative=true,.isStatic=true
        };
        fn->SetReturnType(type);
        for (std::size_t i=0;i<count;++i)
            fn->AddParam("String",args[i]);
        rtti->RegisterFunction(fn);
    };
    const char* one[]={"name"};
    const char* two[]={"modId","settingId"};
    const char* three[]={"modId","settingId","payload"};
    const char* registerArgs[]={"modId","displayName","schemaJson"};

    add("M1Core_Version",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&Version),"String",nullptr,0);
    add("M1Core_RegisterJson",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&Register),"Bool",registerArgs,3);
    add("M1Core_GetJson",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetValue),"String",two,2);
    add("M1Core_SetJson",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetValue),"Bool",three,3);
    add("M1Core_ResetSetting",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ResetValue),"Bool",two,2);
    add("M1Core_ResetMod",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&ResetMod),"Bool",one,1);
    add("M1Core_SaveProfile",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SaveProfile),"Bool",one,1);
    add("M1Core_LoadProfile",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&LoadProfile),"Bool",one,1);
    add("M1Core_DeleteProfile",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&DeleteProfile),"Bool",one,1);
}

std::filesystem::path ModuleDirectory()
{
    HMODULE dll=nullptr;
    const auto address=reinterpret_cast<LPCWSTR>(&ModuleDirectory);
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,address,&dll))
        return {};
    wchar_t buffer[MAX_PATH+1]{};
    const auto size=GetModuleFileNameW(dll,buffer,MAX_PATH);
    return size && size<MAX_PATH ?
        std::filesystem::path(buffer).parent_path() : std::filesystem::path{};
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(
    RED4ext::v1::PluginHandle handle,RED4ext::v1::EMainReason reason,
    const RED4ext::v1::Sdk* sdk)
{
    if (reason==RED4ext::v1::EMainReason::Unload) {
        m1core::RenderBridge::Get().Uninstall();
        return true;
    }
    if (reason!=RED4ext::v1::EMainReason::Load) return true;

    const auto folder=ModuleDirectory();
    if (folder.empty()) return false;
    m1core::Store().Configure(folder / L"settings.json");
    auto* rtti=RED4ext::CRTTISystem::Get();
    if (!rtti) return false;
    rtti->AddRegisterCallback(RegisterTypes);
    rtti->AddPostRegisterCallback(PostRegisterTypes);

    // CET opens first-time bindings only after game base initialization.
    // Use RED4ext's public game-state lifecycle callback, independently.
    if (!sdk || !sdk->gameStates || !sdk->gameStates->Add ||
        !sdk->gameStates->Add(handle, RED4ext::EGameStateType::BaseInitialization,
                             &baseGameCallbacks)) {
        if (sdk && sdk->logger && sdk->logger->Warn)
            sdk->logger->Warn(handle,
                "m1core: base initialization callback unavailable; first-run setup deferred");
    }

    if (!m1core::RenderBridge::Get().Install(handle,sdk)) {
        // A missing or version-incompatible graphics symbol must never
        // prevent Cyberpunk from starting or block its input.
        if (sdk && sdk->logger && sdk->logger->Warn)
            sdk->logger->Warn(handle,
                "m1core: renderer unavailable for this game build; overlay disabled");
    }
    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info)
{
    info->name=L"m1core";
    info->author=L"MISO";
    info->version=RED4EXT_V1_SEMVER(1,0,0);
    info->runtime=RED4EXT_V1_RUNTIME_VERSION_LATEST;
    info->sdk=RED4EXT_V1_SDK_VERSION_CURRENT;
}
RED4EXT_C_EXPORT std::uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
