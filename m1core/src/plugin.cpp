#include "settings.hpp"
#include "overlay.hpp"
#include "diagnostics.hpp"
#include "keybind_actions.hpp"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/CName.hpp>
#include <RED4ext/CNamePool.hpp>

#include <Windows.h>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
HMODULE g_module = nullptr;
std::unique_ptr<m1core::Registry> g_settings;
RED4ext::v1::PluginHandle g_handle{};
const RED4ext::v1::Logger* g_logger = nullptr;

using Frame = RED4ext::CStackFrame;
using Scriptable = RED4ext::IScriptable;
using Name = RED4ext::CName;

template<typename T>
void Output(T* out, T value)
{
    if (out)
        *out = value;
}

void Available(Scriptable*, Frame* frame, bool* out, int64_t)
{
    frame->code++;
    Output(out, g_settings != nullptr);
}

// The core REDscript PlayerPuppet wrapper calls this when real V spawns.
// Do not install input or rendering hooks during the opening title prompt.
void GameplayAttached(Scriptable*, Frame* frame, bool* out, int64_t)
{
    frame->code++;
    m1core::NotifyPlayerAttached();
    Output(out, true);
}

void RegisterBool(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    bool def = false;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &def);
    frame->code++;
    Output(out, g_settings && g_settings->RegisterBool(mod.hash, id.hash, def));
}

void RegisterInt(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    std::int32_t def = 0, min = 0, max = 0;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &def);
    RED4ext::GetParameter(frame, &min);
    RED4ext::GetParameter(frame, &max);
    frame->code++;
    Output(out, g_settings && g_settings->RegisterInt(mod.hash, id.hash, def, min, max));
}

void RegisterFloat(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    float def = 0.0F, min = 0.0F, max = 0.0F;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &def);
    RED4ext::GetParameter(frame, &min);
    RED4ext::GetParameter(frame, &max);
    frame->code++;
    Output(out, g_settings && g_settings->RegisterFloat(mod.hash, id.hash, def, min, max));
}

void RegisterEnumSetting(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    std::int32_t def = 0;
    RED4ext::CString labels;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &def);
    RED4ext::GetParameter(frame, &labels);
    frame->code++;

    std::vector<std::string> choices;
    const std::string options(labels.c_str(), labels.Length());
    if (!options.empty() && options.size() <= 8192)
    {
        std::size_t start = 0;
        while (start <= options.size() && choices.size() <= 64)
        {
            const auto end = options.find('|', start);
            auto label = options.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!label.empty())
                choices.push_back(std::move(label));
            else
            {
                choices.clear();
                break;
            }
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
    }
    Output(out, g_settings && g_settings->RegisterEnum(mod.hash, id.hash, def, std::move(choices)));
}

void RegisterKeybindSetting(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    std::int32_t def = 0;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &def);
    frame->code++;
    Output(out, g_settings && g_settings->RegisterKeybind(mod.hash, id.hash, def));
}

void GetBool(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;
    Output(out, g_settings ? g_settings->GetBool(mod.hash, id.hash).value_or(false) : false);
}

void GetInt(Scriptable*, Frame* frame, std::int32_t* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;
    Output(out, g_settings ? g_settings->GetInt(mod.hash, id.hash).value_or(0) : 0);
}

void GetFloat(Scriptable*, Frame* frame, float* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;
    Output(out, g_settings ? g_settings->GetFloat(mod.hash, id.hash).value_or(0.0F) : 0.0F);
}

void GetEnum(Scriptable*, Frame* frame, std::int32_t* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;
    Output(out, g_settings ? g_settings->GetEnum(mod.hash, id.hash).value_or(0) : 0);
}

void PollKeybindPressed(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;

    // The REDscript mod calls this from its own update. No callback,
    // input interception, or CET dependency is introduced.
    const auto vk = g_settings ? g_settings->GetKeybind(mod.hash, id.hash) : std::nullopt;
    if (!vk)
    {
        Output(out, false);
        return;
    }

    // The player's chosen menu shortcut has priority over mod actions.
    // No particular F-key is reserved by the runtime.
    if (*vk > 0 && *vk == m1core::GetOverlayHotkey()) {
        m1core::KeybindActions::Get().Poll(mod.hash, id.hash, 0, false, false);
        Output(out, false);
        return;
    }
    const HWND foreground = GetForegroundWindow();
    DWORD process = 0;
    if (foreground)
        GetWindowThreadProcessId(foreground, &process);
    const bool allowed = foreground && process == GetCurrentProcessId() &&
                         !m1core::IsOverlayOpen();
    const bool pressed = *vk > 0 && *vk < 256 &&
                         (GetAsyncKeyState(*vk) & 0x8000) != 0;
    Output(out, m1core::KeybindActions::Get().Poll(mod.hash, id.hash,
                                                   *vk, pressed, allowed));
}

void GetKeybind(Scriptable*, Frame* frame, std::int32_t* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;
    Output(out, g_settings ? g_settings->GetKeybind(mod.hash, id.hash).value_or(0) : 0);
}

void SetBool(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    bool value = false;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &value);
    frame->code++;
    Output(out, g_settings && g_settings->SetBool(mod.hash, id.hash, value));
}

void SetInt(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    std::int32_t value = 0;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &value);
    frame->code++;
    Output(out, g_settings && g_settings->SetInt(mod.hash, id.hash, value));
}

void SetFloat(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    float value = 0.0F;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &value);
    frame->code++;
    Output(out, g_settings && g_settings->SetFloat(mod.hash, id.hash, value));
}

void SetEnum(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    std::int32_t value = 0;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &value);
    frame->code++;
    Output(out, g_settings && g_settings->SetEnum(mod.hash, id.hash, value));
}

void SetKeybind(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    std::int32_t value = 0;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &value);
    frame->code++;
    Output(out, g_settings && g_settings->SetKeybind(mod.hash, id.hash, value));
}

void Reset(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;
    Output(out, g_settings && g_settings->Reset(mod.hash, id.hash));
}

void ResetModSettings(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod;
    RED4ext::GetParameter(frame, &mod);
    frame->code++;
    Output(out, g_settings && g_settings->ResetMod(mod.hash));
}

void ResetGroupSettings(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, group;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &group);
    frame->code++;
    Output(out, g_settings && g_settings->ResetGroup(mod.hash, group.hash));
}

void SetSettingGroup(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id, group;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &group);
    frame->code++;
    Output(out, g_settings && g_settings->SetGroup(mod.hash, id.hash, group.hash));
}

void SetDisplayNameNative(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    RED4ext::CString label;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &label);
    frame->code++;
    Output(out, g_settings && g_settings->SetDisplayName(
        mod.hash, id.hash, std::string_view(label.c_str(), label.Length())));
}

void SetDescriptionNative(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod, id;
    RED4ext::CString description;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    RED4ext::GetParameter(frame, &description);
    frame->code++;
    Output(out, g_settings && g_settings->SetDescription(
        mod.hash, id.hash, std::string_view(description.c_str(), description.Length())));
}

void GetSettingRevisionNative(Scriptable*, Frame* frame, std::int32_t* out, int64_t)
{
    Name mod, id;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &id);
    frame->code++;
    Output(out, g_settings ? g_settings->Revision(mod.hash, id.hash).value_or(-1) : -1);
}

void WriteLog(Scriptable*, Frame* frame, bool* out, int64_t)
{
    Name mod;
    std::int32_t level = 0;
    RED4ext::CString message;
    RED4ext::GetParameter(frame, &mod);
    RED4ext::GetParameter(frame, &level);
    RED4ext::GetParameter(frame, &message);
    frame->code++;
    if (!mod.hash || level < 0 || level > 3 || message.Length() == 0 ||
        message.Length() > 2048)
    {
        Output(out, false);
        return;
    }
    const char* source = RED4ext::CNamePool::Get(mod);
    Output(out, m1core::Diagnostics::Get().Write(
        static_cast<m1core::LogLevel>(level),
        source && *source ? source : "unknown-mod",
        std::string_view(message.c_str(), message.Length())));
}

void Save(Scriptable*, Frame* frame, bool* out, int64_t)
{
    frame->code++;
    Output(out, g_settings && g_settings->Save());
}

template<typename T>
void RegisterNative(const char* name, RED4ext::ScriptingFunction_t<T> function,
                    std::initializer_list<std::pair<const char*, const char*>> params,
                    const char* returnType)
{
    auto* native = RED4ext::CGlobalFunction::Create(name, name, function);
    if (!native)
        return;
    native->flags = {.isNative = true, .isStatic = true, .isPublic = true};
    for (const auto& [type, parameter] : params)
        native->AddParam(type, parameter);
    native->SetReturnType(returnType);
    RED4ext::CRTTISystem::Get()->RegisterFunction(native);
}

void RegisterNatives()
{
    RegisterNative("M1Core_IsAvailable", &Available, {}, "Bool");
    RegisterNative("M1Core_GameplayAttached", &GameplayAttached, {}, "Bool");
    RegisterNative("M1Core_RegisterBoolSetting", &RegisterBool,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Bool", "defaultValue"}}, "Bool");
    RegisterNative("M1Core_RegisterIntSetting", &RegisterInt,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Int32", "defaultValue"},
                    {"Int32", "minValue"}, {"Int32", "maxValue"}}, "Bool");
    RegisterNative("M1Core_RegisterFloatSetting", &RegisterFloat,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Float", "defaultValue"},
                    {"Float", "minValue"}, {"Float", "maxValue"}}, "Bool");
    RegisterNative("M1Core_RegisterEnumSetting", &RegisterEnumSetting,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Int32", "defaultIndex"},
                    {"String", "choices"}}, "Bool");
    RegisterNative("M1Core_RegisterKeybindSetting", &RegisterKeybindSetting,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Int32", "defaultVirtualKey"}}, "Bool");
    RegisterNative("M1Core_GetEnumSetting", &GetEnum,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Int32");
    RegisterNative("M1Core_PollKeybindPressed", &PollKeybindPressed,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Bool");
    RegisterNative("M1Core_GetKeybindSetting", &GetKeybind,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Int32");
    RegisterNative("M1Core_SetEnumSetting", &SetEnum,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Int32", "index"}}, "Bool");
    RegisterNative("M1Core_SetKeybindSetting", &SetKeybind,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Int32", "virtualKey"}}, "Bool");
    RegisterNative("M1Core_GetBoolSetting", &GetBool,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Bool");
    RegisterNative("M1Core_GetIntSetting", &GetInt,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Int32");
    RegisterNative("M1Core_GetFloatSetting", &GetFloat,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Float");
    RegisterNative("M1Core_SetBoolSetting", &SetBool,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Bool", "value"}}, "Bool");
    RegisterNative("M1Core_SetIntSetting", &SetInt,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Int32", "value"}}, "Bool");
    RegisterNative("M1Core_SetFloatSetting", &SetFloat,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"Float", "value"}}, "Bool");
    RegisterNative("M1Core_ResetSetting", &Reset,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Bool");
    RegisterNative("M1Core_ResetModSettings", &ResetModSettings,
                   {{"CName", "modName"}}, "Bool");
    RegisterNative("M1Core_ResetGroupSettings", &ResetGroupSettings,
                   {{"CName", "modName"}, {"CName", "groupName"}}, "Bool");
    RegisterNative("M1Core_SetSettingGroup", &SetSettingGroup,
                   {{"CName", "modName"}, {"CName", "settingId"},
                    {"CName", "groupName"}}, "Bool");
    RegisterNative("M1Core_SetSettingDisplayName", &SetDisplayNameNative,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"String", "displayName"}}, "Bool");
    RegisterNative("M1Core_SetSettingDescription", &SetDescriptionNative,
                   {{"CName", "modName"}, {"CName", "settingId"}, {"String", "description"}}, "Bool");
    RegisterNative("M1Core_GetSettingRevision", &GetSettingRevisionNative,
                   {{"CName", "modName"}, {"CName", "settingId"}}, "Int32");
    RegisterNative("M1Core_LogEvent", &WriteLog,
                   {{"CName", "modName"}, {"Int32", "level"}, {"String", "message"}}, "Bool");
    RegisterNative("M1Core_SaveSettings", &Save, {}, "Bool");
}

std::filesystem::path ConfigPath()
{
    wchar_t path[MAX_PATH]{};
    if (!g_module || !GetModuleFileNameW(g_module, path, MAX_PATH))
        return {};
    return std::filesystem::path(path).parent_path() / L"settings.json";
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        g_module = module;
    return TRUE;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info)
{
    info->name = L"m1core";
    info->author = L"m1nsook";
    info->version = RED4EXT_V1_SEMVER(0, 5, 0);
    info->runtime = RED4EXT_V1_RUNTIME_VERSION_2_50;
    info->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_1;
}

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle handle,
                                         RED4ext::v1::EMainReason reason,
                                         const RED4ext::v1::Sdk* sdk)
{
    switch (reason)
    {
    case RED4ext::v1::EMainReason::Load:
    {
        g_handle = handle;
        g_logger = sdk ? sdk->logger : nullptr;
        const auto config = ConfigPath();
        if (config.empty())
        {
            if (g_logger)
                g_logger->Error(g_handle, "m1core: unable to resolve plugin directory");
            return false;
        }
        m1core::Diagnostics::Get().Initialize(config.parent_path());
        m1core::Diagnostics::Get().Write(m1core::LogLevel::Info, "m1core",
                                         "v0.5.0-dev loading; Cyberpunk 2.50");
        g_settings = std::make_unique<m1core::Registry>(config);
        if (!g_settings->Load())
        {
            if (g_logger)
                g_logger->Warn(g_handle, "m1core: settings.json could not be read; defaults will be used");
            m1core::Diagnostics::Get().Write(m1core::LogLevel::Warning, "m1core",
                                              "settings.json unreadable; using defaults");
        }
        if (!g_settings->LoadProfiles())
        {
            if (g_logger)
                g_logger->Warn(g_handle, "m1core: profiles.json could not be read; profiles unavailable");
            m1core::Diagnostics::Get().Write(m1core::LogLevel::Warning, "m1core",
                                              "profiles.json unreadable; profiles unavailable");
        }
        RED4ext::CRTTISystem::Get()->AddPostRegisterCallback(RegisterNatives);
        m1core::StartOverlay(*g_settings, config.parent_path(), handle, sdk);
        if (g_logger)
            g_logger->Info(g_handle, "m1core 0.5.0-dev: settings API active; renderer opt-in");
        return true;
    }
    case RED4ext::v1::EMainReason::Unload:
        m1core::StopOverlay();
        if (g_settings && !g_settings->Save())
        {
            if (g_logger)
                g_logger->Warn(g_handle, "m1core: settings save failed");
            m1core::Diagnostics::Get().Write(m1core::LogLevel::Error, "m1core",
                                              "settings save failed during unload");
        }
        m1core::Diagnostics::Get().Write(m1core::LogLevel::Info, "m1core", "plugin unloaded");
        g_settings.reset();
        m1core::KeybindActions::Get().Reset();
        return true;
    }
    return true;
}
