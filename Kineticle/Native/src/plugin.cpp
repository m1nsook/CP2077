// Kineticle native development probe. No gameplay writes in this build.
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/vehicleBaseObject.hpp>
#include <RED4ext/Scripting/Natives/vehiclePhysicsData.hpp>
#include <Windows.h>
#include <winver.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "physics.hpp"

namespace {
using RED4ext::IScriptable;
using RED4ext::CStackFrame;
using RED4ext::CString;
using PhysicsData=RED4ext::vehicle::PhysicsData;
using Json=nlohmann::json;
constexpr const char* version="0.1.0-dev.6-native-probe.1";
constexpr std::uint32_t syncHash=547299556u;
using Sync=void(*)(void*,void*,PhysicsData*,float,float);
Sync original=nullptr;
void* target=nullptr;
std::atomic<bool> enabled{true}, installed{false};
std::atomic<std::uint64_t> calls{0}, candidateFrames{0}, corrections{0};
std::mutex stateMutex, sampleMutex;
std::filesystem::path configPath;
std::string status="not initialized";
struct Sample { kineticle::Vec before{},after{},angularBefore{},angularAfter{}; float loss{}; };
Sample last;
std::string moduleVersion(HMODULE module) {
    wchar_t path[MAX_PATH+1]{};
    DWORD unused=0;
    if (!GetModuleFileNameW(module,path,MAX_PATH)) return {};
    auto size=GetFileVersionInfoSizeW(path,&unused);
    if (!size||size>1048576) return {};
    std::vector<unsigned char> bytes(size);
    if (!GetFileVersionInfoW(path,0,size,bytes.data())) return {};
    VS_FIXEDFILEINFO* info=nullptr; UINT count=0;
    if (!VerQueryValueW(bytes.data(),L"\\",reinterpret_cast<void**>(&info),&count)
        ||!info||count<sizeof(VS_FIXEDFILEINFO)) return {};
    return std::to_string(HIWORD(info->dwFileVersionMS))+"."+std::to_string(LOWORD(info->dwFileVersionMS))+"."
        +std::to_string(HIWORD(info->dwFileVersionLS))+"."+std::to_string(LOWORD(info->dwFileVersionLS));
}
std::filesystem::path moduleDirectory() {
    HMODULE mod=nullptr;
    wchar_t path[MAX_PATH+1]{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&moduleDirectory),&mod)) return {};
    const auto n=GetModuleFileNameW(mod,path,MAX_PATH);
    return n&&n<MAX_PATH?std::filesystem::path(path).parent_path():std::filesystem::path{};
}
bool save(bool value) {
    try {
        if(configPath.empty()) return false;
        auto tmp=configPath; tmp+=L".tmp";
        {std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
         out<<Json{{"format",1},{"probeEnabled",value}}.dump(2)<<"\n";
         out.flush(); if(!out) return false;}
        if(!MoveFileExW(tmp.c_str(),configPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return false;
        enabled.store(value);return true;
    } catch(...) {return false;}
}
bool load() {
    try {
        const auto folder=moduleDirectory();
        if(folder.empty()) return false;
        configPath=folder/L"config.json";
        if(!std::filesystem::exists(configPath)) return save(true);
        std::ifstream in(configPath);
        auto j=Json::parse(in);
        if(j.at("format")!=1||!j.at("probeEnabled").is_boolean()) return false;
        enabled.store(j.at("probeEnabled").get<bool>());return true;
    } catch(...) {return false;}
}
kineticle::Vec vec(const RED4ext::Vector3& v){return {v.X,v.Y,v.Z};}
void sync(void* self,void* chassis,PhysicsData* rb,float a4,float dt) {
    if (!enabled.load(std::memory_order_relaxed)||!rb||!chassis) { original(self,chassis,rb,a4,dt);return; }
    calls.fetch_add(1,std::memory_order_relaxed);
    // isPlayerControlled is an SDK candidate flag, NOT proof of V being the driver.
    const auto* owner=rb->vehicle;
    if (!owner||!owner->isPlayerControlled||owner->physicsData!=rb) { original(self,chassis,rb,a4,dt);return; }
    const auto before=vec(rb->velocity), angularBefore=vec(rb->angularVelocity);
    original(self,chassis,rb,a4,dt);
    candidateFrames.fetch_add(1,std::memory_order_relaxed);
    const auto after=vec(rb->velocity), angularAfter=vec(rb->angularVelocity);
    if(!kineticle::valid(before)||!kineticle::valid(after)||!kineticle::valid(angularBefore)||!kineticle::valid(angularAfter)) return;
    const auto loss=kineticle::forwardLoss(before,after);
    if(loss<0.5f) return;
    std::lock_guard lock(sampleMutex);
    last={before,after,angularBefore,angularAfter,loss};
    corrections.fetch_add(1,std::memory_order_relaxed);
}
bool attach(RED4ext::PluginHandle handle,const RED4ext::Sdk* sdk) {
    if(moduleVersion(nullptr)!="3.0.88.46235"){status="unsupported executable; hook inactive";return false;}
    auto loader=GetModuleHandleW(L"RED4ext.dll");
    if(!loader||moduleVersion(loader)!="1.31.1.0"){status="unsupported RED4ext; hook inactive";return false;}
    if(!sdk||!sdk->hooking||!sdk->hooking->Attach){status="hook API unavailable";return false;}
    // Unlike UniversalRelocBase::Resolve, a missing hash must not terminate the game.
    using Resolver=std::uintptr_t(*)(std::uint32_t);
    auto resolve=reinterpret_cast<Resolver>(GetProcAddress(loader,"RED4ext_ResolveAddress"));
    if(!resolve){status="address resolver unavailable";return false;}
    target=reinterpret_cast<void*>(resolve(syncHash));
    MEMORY_BASIC_INFORMATION region{};
    if(!target||!VirtualQuery(target,&region,sizeof(region))||region.State!=MEM_COMMIT||
        region.AllocationBase!=GetModuleHandleW(nullptr)||(region.Protect&PAGE_GUARD)||
        !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){
        target=nullptr;status="physics address unavailable";return false;
    }
    if(!sdk->hooking->Attach(handle,target,reinterpret_cast<void*>(&sync),reinterpret_cast<void**>(&original))){
        target=nullptr;status="physics hook attach failed";return false;
    }
    installed.store(true);
    status="read-only PhysX sync hook installed; no collision writes";
    return true;
}
std::string diagnostic() {
    std::lock_guard lock(sampleMutex);
    std::ostringstream s;
    s<<version<<"; "<<status<<"; enabled="<<enabled.load()<<"; sync calls="<<calls.load()
     <<"; player-controlled candidate frames="<<candidateFrames.load()<<"; forward-loss samples="<<corrections.load()
     <<"; last velocity before=("<<last.before.x<<","<<last.before.y<<","<<last.before.z<<")"
     <<"; after=("<<last.after.x<<","<<last.after.y<<","<<last.after.z<<")"
     <<"; angular before=("<<last.angularBefore.x<<","<<last.angularBefore.y<<","<<last.angularBefore.z<<")"
     <<"; after=("<<last.angularAfter.x<<","<<last.angularAfter.y<<","<<last.angularAfter.z<<")"
     <<"; forward loss="<<last.loss<<"; contact pair unverified; damage protection inactive";
    return s.str();
}
void getEnabled(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){frame->code++;if(out)*out=enabled.load();}
void setEnabled(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){
    bool value=false;RED4ext::GetParameter(frame,&value);frame->code++;
    std::lock_guard lock(stateMutex);const auto ok=save(value);if(out)*out=ok;
}
void getReport(IScriptable*,CStackFrame* frame,CString* out,std::int64_t){
    frame->code++;if(out)*out=CString(diagnostic().c_str());
}
void resetReport(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){
    frame->code++;std::lock_guard lock(sampleMutex);
    calls.store(0);candidateFrames.store(0);corrections.store(0);last={};if(out)*out=true;
}
void registerTypes(){}
void postRegister(){
    auto* rtti=RED4ext::CRTTISystem::Get();if(!rtti)return;
    const auto add=[&](const char* name,RED4ext::ScriptingFunction_t<void*> callback,const char* result,bool arg){
        auto* fn=RED4ext::CGlobalFunction::Create(name,name,callback);
        if(!fn)return;
        fn->flags=RED4ext::CBaseFunction::Flags{.isNative=true,.isStatic=true};
        fn->SetReturnType(result);if(arg)fn->AddParam("Bool","enabled");
        rtti->RegisterFunction(fn);
    };
    add("Kineticle_GetProbeEnabled",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getEnabled),"Bool",false);
    add("Kineticle_SetProbeEnabled",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&setEnabled),"Bool",true);
    add("Kineticle_Diagnostics",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getReport),"String",false);
    add("Kineticle_ResetDiagnostics",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&resetReport),"Bool",false);
}
}
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle handle,RED4ext::EMainReason reason,const RED4ext::Sdk* sdk){
    if(reason==RED4ext::EMainReason::Unload){
        enabled.store(false);
        if(installed.load()&&sdk&&sdk->hooking&&sdk->hooking->Detach){
            sdk->hooking->Detach(handle,target);installed.store(false);
        }
        return true;
    }
    if(reason!=RED4ext::EMainReason::Load)return true;
    auto* rtti=RED4ext::CRTTISystem::Get();if(!rtti)return false;
    rtti->AddRegisterCallback(registerTypes);rtti->AddPostRegisterCallback(postRegister);
    if(load()) attach(handle,sdk);
    else {enabled.store(false);status="config invalid or inaccessible; hook inactive";}
    if(sdk&&sdk->logger&&sdk->logger->Info){const auto message=diagnostic();sdk->logger->Info(handle,message.c_str());}
    return true;
}
RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::PluginInfo* info){
    info->name=L"Kineticle";info->author=L"MISO";info->version=RED4EXT_SEMVER(0,1,0);
    info->runtime=RED4EXT_RUNTIME_2_50;info->sdk=RED4EXT_SDK_LATEST;
}
RED4EXT_C_EXPORT std::uint32_t RED4EXT_CALL Supports(){return RED4EXT_API_VERSION_LATEST;}
