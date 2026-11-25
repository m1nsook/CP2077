// Kineticle native development probe. No gameplay writes in this build.
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/vehicleBaseObject.hpp>
#include <RED4ext/Scripting/Natives/vehiclePhysicsData.hpp>
#include <RED4ext/Scripting/Natives/Generated/vehicle/VehicleBumpEvent.hpp>
#include <RED4ext/Scripting/Utils.hpp>
#include <RED4ext/Scripting/CProperty.hpp>
#include <RED4ext/Detail/AddressHashes.hpp>
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
constexpr const char* version="0.1.0-dev.6-native-probe.2";
constexpr std::uint32_t syncHash=547299556u;
using Sync=void(*)(void*,void*,PhysicsData*,float,float);
Sync original=nullptr;
void* target=nullptr;
// The SDK's CBaseFunction::Execute wrapper supplies this exact signature.
// Observe only the engine CStack layout, never CScriptStack or VM bytecode.
using ExecuteScripted=bool(*)(RED4ext::CBaseFunction*,RED4ext::CStack*,void*);
ExecuteScripted originalScripted=nullptr;
void* eventTarget=nullptr;
std::uintptr_t stackVtable=0;
std::atomic<bool> eventInstalled{false};
std::atomic<std::uint64_t> bumpCalls{0}, rejectedStacks{0}, invalidPairs{0},
    observedPairs{0}, npcPairs{0}, driverQueryFailures{0}, playerPairs{0},
    flagDisagreements{0}, guardedPairs{0};
std::string eventStatus="contact hook inactive";
struct PairSample {
    std::uint64_t receiver{},other{},syncSequence{};
    int receiverDriver{-1},otherDriver{-1},remote{-1},quest{-1},destroyed{-1};
    bool receiverCandidate{},otherCandidate{},roadVehicle{};
    float delta{};
    kineticle::Vec normal{};
    DWORD thread{};
};
PairSample lastPair;
std::atomic<bool> enabled{true}, installed{false};
std::atomic<std::uint64_t> calls{0}, candidateFrames{0}, corrections{0};
std::mutex stateMutex, sampleMutex;
std::filesystem::path configPath;
std::string status="not initialized";
struct Sample { kineticle::Vec before{},after{},angularBefore{},angularAfter{}; float loss{}; std::uint64_t sequence{}; };
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
    last={before,after,angularBefore,angularAfter,loss,calls.load(std::memory_order_relaxed)};
    corrections.fetch_add(1,std::memory_order_relaxed);
}
// Only query zero-argument native Bool methods from a script event context.
// Do not invoke RTTI from the PhysX sync callback.
int readNativeBool(RED4ext::vehicle::BaseObject* object,const char* name) {
    auto* type=object->GetType();
    auto* fn=type?type->GetFunction(RED4ext::CName(name)):nullptr;
    if(!fn||!fn->flags.isNative||fn->flags.isStatic||fn->params.size!=0||
       !fn->returnType||!fn->returnType->type||
       fn->returnType->type->GetName()!=RED4ext::CName("Bool")) return -1;
    bool result=false;
    return RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(object),fn,&result)
        ? (result?1:0) : -1;
}
bool roadVehicle(RED4ext::vehicle::BaseObject* v,RED4ext::CRTTISystem* rtti) {
    auto* type=v->GetType();
    auto* car=rtti->GetClass("vehicleCarBaseObject");
    auto* bike=rtti->GetClass("vehicleBikeBaseObject");
    return type&&((car&&type->IsA(car))||(bike&&type->IsA(bike)));
}
void observeBump(RED4ext::CBaseFunction* fn,RED4ext::CStack* stack) {
    bumpCalls.fetch_add(1,std::memory_order_relaxed);
    if(!stack||*reinterpret_cast<const std::uintptr_t*>(stack)!=stackVtable) {
        rejectedStacks.fetch_add(1,std::memory_order_relaxed);return;
    }
    auto* rtti=RED4ext::CRTTISystem::Get();
    auto* vehicleType=rtti?rtti->GetClass("vehicleBaseObject"):nullptr;
    auto* bumpType=rtti?rtti->GetClass("vehicleVehicleBumpEvent"):nullptr;
    auto* context=stack->GetContext();
    if(!vehicleType||!bumpType||!context||!context->GetType()||
       !context->GetType()->IsA(vehicleType)||fn->params.size!=1||
       !stack->args||stack->argsCount!=1) {
        invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    auto& arg=stack->args[0];
    if(!arg.type||!arg.value||!fn->params[0]||arg.type!=fn->params[0]->type||
       arg.type->GetType()!=RED4ext::ERTTIType::Handle||
       static_cast<RED4ext::CRTTIHandleType*>(arg.type)->GetInnerType()!=bumpType) {
        invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    const auto& eventHandle=*static_cast<const RED4ext::Handle<IScriptable>*>(arg.value);
    auto* event=eventHandle.GetPtr();
    if(!event||!event->GetType()||!event->GetType()->IsA(bumpType)) {
        invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    auto* bump=static_cast<RED4ext::vehicle::VehicleBumpEvent*>(event);
    auto otherHandle=bump->hitVehicle.Lock();
    auto* other=otherHandle.GetPtr();
    auto* receiver=static_cast<RED4ext::vehicle::BaseObject*>(context);
    if(!other||other==receiver||!other->GetType()||!other->GetType()->IsA(vehicleType)||
       !receiver->entityID.hash||!other->entityID.hash||receiver->entityID.hash==other->entityID.hash||
       !std::isfinite(bump->impactVelocityChange)||!kineticle::valid(vec(bump->hitNormal))) {
        invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    observedPairs.fetch_add(1,std::memory_order_relaxed);
    PairSample sample;
    sample.receiver=receiver->entityID.hash;sample.other=other->entityID.hash;
    sample.receiverCandidate=receiver->isPlayerControlled!=0;
    sample.otherCandidate=other->isPlayerControlled!=0;
    sample.delta=bump->impactVelocityChange;sample.normal=vec(bump->hitNormal);
    sample.syncSequence=calls.load(std::memory_order_relaxed);
    sample.thread=GetCurrentThreadId();
    sample.receiverDriver=readNativeBool(receiver,"IsPlayerDriver");
    sample.otherDriver=readNativeBool(other,"IsPlayerDriver");
    if(sample.receiverDriver<0||sample.otherDriver<0)
        driverQueryFailures.fetch_add(1,std::memory_order_relaxed);
    if((sample.receiverDriver>=0&&sample.receiverCandidate!=(sample.receiverDriver==1))||
       (sample.otherDriver>=0&&sample.otherCandidate!=(sample.otherDriver==1)))
        flagDisagreements.fetch_add(1,std::memory_order_relaxed);
    if(sample.receiverDriver==0&&sample.otherDriver==0&&!sample.receiverCandidate&&!sample.otherCandidate) {
        npcPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    // Do not treat a candidate flag, missing native function, or two positive
    // driver checks as proof. Only one confirmed player driver is acceptable.
    if((sample.receiverDriver==1&&sample.otherDriver==0)||
       (sample.otherDriver==1&&sample.receiverDriver==0)) {
        playerPairs.fetch_add(1,std::memory_order_relaxed);
        auto* driver=sample.receiverDriver==1?receiver:other;
        sample.remote=readNativeBool(driver,"IsVehicleRemoteControlled");
        sample.quest=readNativeBool(driver,"IsInQuestOrScenePhysicsState");
        sample.destroyed=readNativeBool(driver,"IsDestroyed");
        sample.roadVehicle=roadVehicle(driver,rtti);
        if(sample.remote==0&&sample.quest==0&&sample.destroyed==0&&sample.roadVehicle)
            guardedPairs.fetch_add(1,std::memory_order_relaxed);
    }
    // Store only numeric observations, never keep a vehicle pointer across callbacks.
    std::lock_guard lock(sampleMutex);lastPair=sample;
}
bool executeScripted(RED4ext::CBaseFunction* fn,RED4ext::CStack* stack,void* a3) {
    if(enabled.load(std::memory_order_relaxed)&&fn&&
       fn->shortName==RED4ext::CName("OnVehicleBumpEvent")) {
        // A native Bool query should not reenter this event, but fail closed if it does.
        static thread_local bool observing=false;
        if(!observing) {
            observing=true;
            observeBump(fn,stack);
            observing=false;
        }
    }
    return originalScripted(fn,stack,a3);
}
bool executableAddress(void* address) {
    MEMORY_BASIC_INFORMATION region{};
    return address&&VirtualQuery(address,&region,sizeof(region))&&region.State==MEM_COMMIT&&
        region.AllocationBase==GetModuleHandleW(nullptr)&&!(region.Protect&PAGE_GUARD)&&
        (region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
}
using Resolver=std::uintptr_t(*)(std::uint32_t);
void attachEvents(RED4ext::PluginHandle handle,const RED4ext::Sdk* sdk,Resolver resolve) {
    eventTarget=reinterpret_cast<void*>(resolve(RED4ext::Detail::AddressHashes::CBaseFunction_ExecuteScripted));
    stackVtable=resolve(RED4ext::Detail::AddressHashes::CStack_vtbl);
    MEMORY_BASIC_INFORMATION region{};
    if(!executableAddress(eventTarget)||!stackVtable||
       !VirtualQuery(reinterpret_cast<void*>(stackVtable),&region,sizeof(region))||
       region.State!=MEM_COMMIT||region.AllocationBase!=GetModuleHandleW(nullptr)||
       (region.Protect&(PAGE_GUARD|PAGE_NOACCESS))) {
        eventTarget=nullptr;eventStatus="contact addresses unavailable";return;
    }
    if(!sdk->hooking->Attach(handle,eventTarget,reinterpret_cast<void*>(&executeScripted),
                              reinterpret_cast<void**>(&originalScripted))) {
        eventTarget=nullptr;eventStatus="contact hook attach failed";return;
    }
    eventInstalled.store(true);
    eventStatus="read-only scripted event dispatcher hook installed; coverage unverified";
}
bool attach(RED4ext::PluginHandle handle,const RED4ext::Sdk* sdk) {
    if(moduleVersion(nullptr)!="3.0.88.46235"){status="unsupported executable; hook inactive";return false;}
    auto loader=GetModuleHandleW(L"RED4ext.dll");
    if(!loader||moduleVersion(loader)!="1.31.1.0"){status="unsupported RED4ext; hook inactive";return false;}
    if(!sdk||!sdk->hooking||!sdk->hooking->Attach){status="hook API unavailable";return false;}
    // Unlike UniversalRelocBase::Resolve, a missing hash must not terminate the game.
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
    attachEvents(handle,sdk,resolve);
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
     <<"; forward loss="<<last.loss<<"; loss sample sync sequence="<<last.sequence
     <<"; "<<eventStatus<<"; bump callbacks="<<bumpCalls.load()
     <<"; rejected stacks="<<rejectedStacks.load()<<"; invalid pairs="<<invalidPairs.load()
     <<"; observed vehicle pairs="<<observedPairs.load()<<"; NPC pairs="<<npcPairs.load()
     <<"; driver query failures="<<driverQueryFailures.load()
     <<"; player driver pairs="<<playerPairs.load()<<"; candidate flag disagreements="<<flagDisagreements.load()
     <<"; pairs passing native guards="<<guardedPairs.load()
     <<"; last pair receiver="<<lastPair.receiver<<"; other="<<lastPair.other
     <<"; receiver candidate="<<lastPair.receiverCandidate<<"; other candidate="<<lastPair.otherCandidate
     <<"; receiver IsPlayerDriver="<<lastPair.receiverDriver<<"; other IsPlayerDriver="<<lastPair.otherDriver
     <<"; driver remote="<<lastPair.remote<<"; quest="<<lastPair.quest<<"; destroyed="<<lastPair.destroyed
     <<"; driver car/bike="<<lastPair.roadVehicle<<"; impact delta="<<lastPair.delta
     <<"; hit normal=("<<lastPair.normal.x<<","<<lastPair.normal.y<<","<<lastPair.normal.z<<")"
     <<"; event sync sequence="<<lastPair.syncSequence<<"; event thread="<<lastPair.thread
     <<"; PSM and exact contact-to-sync correlation unverified; all gameplay writes inactive";
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
    calls.store(0);candidateFrames.store(0);corrections.store(0);last={};
    bumpCalls.store(0);rejectedStacks.store(0);invalidPairs.store(0);observedPairs.store(0);
    npcPairs.store(0);driverQueryFailures.store(0);playerPairs.store(0);flagDisagreements.store(0);
    guardedPairs.store(0);lastPair={};if(out)*out=true;
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
        if(eventInstalled.load()&&sdk&&sdk->hooking&&sdk->hooking->Detach){
            sdk->hooking->Detach(handle,eventTarget);eventInstalled.store(false);
        }
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
