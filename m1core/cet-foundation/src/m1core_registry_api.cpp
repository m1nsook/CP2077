// Preserve the existing MIT native settings ABI on the CET-derived core.
// These are the same REDscript entry points shipped by the earlier m1core
// development DLL; Dear ImGui/input now come from the gifted CET foundation.
#include "m1core_registry_api.hpp"
#include "registry.hpp"
#include "state.hpp"

#include <RED4ext/RED4ext.hpp>
#include <cstdint>
#include <string>

namespace {
using RED4ext::CString;
using RED4ext::CStackFrame;
using RED4ext::IScriptable;

void Version(IScriptable*,CStackFrame* frame,CString* output,std::int64_t)
{
    frame->code++;
    if (output) *output=CString("1.0.0-dev.4");
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


} // namespace

namespace m1core {
void RegisterNativeModAPI()
{
    auto* rtti = RED4ext::CRTTISystem::Get();
    if (!rtti) return;
    rtti->AddRegisterCallback(RegisterTypes);
    rtti->AddPostRegisterCallback(PostRegisterTypes);
}
} // namespace m1core
