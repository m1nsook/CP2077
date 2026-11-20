// New DLL entrypoint: native state and REDengine RTTI bridge only.
// CET owns all ImGui rendering, cursor visibility, keybindings, window
// focus and keyboard capture. Do not install another D3D/window hook.
#include "state.hpp"

#include <RED4ext/RED4ext.hpp>
#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {
void GetVersion(RED4ext::IScriptable*, RED4ext::CStackFrame* frame,
                RED4ext::CString* output, std::int64_t)
{
    frame->code++;
    if (output) *output = RED4ext::CString("1.0.0-dev.2");
}
void IsReady(RED4ext::IScriptable*, RED4ext::CStackFrame* frame,
             bool* output, std::int64_t)
{
    frame->code++;
    if (output) *output = m1core::Store().Ready();
}
void GetState(RED4ext::IScriptable*, RED4ext::CStackFrame* frame,
              RED4ext::CString* output, std::int64_t)
{
    frame->code++;
    if (output) {
        const auto value = m1core::Store().Snapshot();
        *output = RED4ext::CString(value.c_str());
    }
}
void SetState(RED4ext::IScriptable*, RED4ext::CStackFrame* frame,
              bool* output, std::int64_t)
{
    RED4ext::CString state;
    RED4ext::GetParameter(frame, &state);
    frame->code++;
    if (output) *output = m1core::Store().Replace(state.c_str());
}
void GetRevision(RED4ext::IScriptable*, RED4ext::CStackFrame* frame,
                 std::int32_t* output, std::int64_t)
{
    frame->code++;
    if (output) *output = m1core::Store().Revision();
}
void GetLastError(RED4ext::IScriptable*, RED4ext::CStackFrame* frame,
                  RED4ext::CString* output, std::int64_t)
{
    frame->code++;
    if (output) {
        const auto value = m1core::Store().LastError();
        *output = RED4ext::CString(value.c_str());
    }
}
void RegisterTypes() {}
void PostRegisterTypes()
{
    auto* rtti = RED4ext::CRTTISystem::Get();
    if (!rtti) return;
    auto add = [&](const char* name, RED4ext::ScriptingFunction_t<void*> callback,
                   const char* resultType, bool hasStringArg = false) {
        auto* fn = RED4ext::CGlobalFunction::Create(name, name, callback);
        fn->flags = RED4ext::CBaseFunction::Flags{ .isNative = true, .isStatic = true };
        fn->SetReturnType(resultType);
        if (hasStringArg) fn->AddParam("String", "payload");
        rtti->RegisterFunction(fn);
    };
    add("M1Core_NativeVersion", reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetVersion), "String");
    add("M1Core_NativeReady", reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&IsReady), "Bool");
    add("M1Core_NativeGetState", reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetState), "String");
    add("M1Core_NativeSetState", reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&SetState), "Bool", true);
    add("M1Core_NativeRevision", reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetRevision), "Int32");
    add("M1Core_NativeLastError", reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&GetLastError), "String");
}
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(
    RED4ext::v1::PluginHandle, RED4ext::v1::EMainReason reason,
    const RED4ext::v1::Sdk*)
{
    if (reason == RED4ext::v1::EMainReason::Load) {
        HMODULE dll = nullptr;
        const auto address = reinterpret_cast<LPCWSTR>(&Main);
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, address, &dll))
            return false;
        wchar_t path[MAX_PATH + 1]{};
        const DWORD length = GetModuleFileNameW(dll, path, MAX_PATH);
        if (!length || length >= MAX_PATH) return false;
        m1core::Store().Configure(
            std::filesystem::path(path).parent_path() / L"settings.json");
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti) return false;
        rtti->AddRegisterCallback(RegisterTypes);
        rtti->AddPostRegisterCallback(PostRegisterTypes);
    }
    return true;
}
RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info)
{
    info->name = L"m1core";
    info->author = L"MISO";
    info->version = RED4EXT_V1_SEMVER(1, 0, 0);
    info->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    info->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}
RED4EXT_C_EXPORT std::uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}
