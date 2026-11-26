// Kineticle standalone Core: native chassis mass, scoped protection, configuration.
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Scripting/Natives/vehicleBaseObject.hpp>
#include <RED4ext/Scripting/Natives/vehiclePhysicsData.hpp>
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
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "report_pages.hpp"
#include "protection_policy.hpp"
#include "mass_policy.hpp"
#include <cstring>
#include <cstddef>
#include <RED4ext/GameEngine.hpp>
#include <RED4ext/GameStates.hpp>
#include <RED4ext/Scripting/Natives/IGameInstance.hpp>
#include <iomanip>
#include <chrono>

namespace {
using RED4ext::IScriptable;
using RED4ext::CStackFrame;
using RED4ext::CString;
using PhysicsData=RED4ext::vehicle::PhysicsData;
using Json=nlohmann::json;
constexpr const char* version="0.1.0-dev.14-standalone";
constexpr std::uint32_t syncHash=547299556u;
using Sync=void(*)(void*,void*,PhysicsData*,float,float);
Sync original=nullptr;
void* target=nullptr;
// The pinned SDK's ExecuteScripted hash is also CET's CScript_RunPureScript
// (3791200470). The dispatcher accepts both array and packed argument stacks.
// Source: CET d7c378d4e9c7b15ca1ce42c37e8732d3b397b047,
// src/scripting/FunctionOverride.cpp (HookRunPureScriptFunction).
// No CET dependency or VM bytecode decoding.
using ExecuteScripted=bool(*)(RED4ext::CBaseFunction*,RED4ext::CScriptStack*,void*);
ExecuteScripted originalScripted=nullptr;
void* eventTarget=nullptr;
std::uintptr_t stackVtable=0;
std::atomic<bool> eventInstalled{false};
std::string eventStatus="protection hook inactive";
enum class Reject : unsigned { None, StackMemory, MissingParam, ParamLayout, ArrayCount,
    ArrayMemory, ArrayType, PackedLayout, ArgumentMemory, EventObject, Count };
struct DecodeSample {
    Reject reason{Reject::None};
    bool array{},knownVtable{},hasResult{},hasResultType{},hasArgType{},argTypeMatches{},hasArgs{};
    bool outParam{};
    std::uint32_t paramSize{},paramOffset{};
    int stackSize{};
};
struct BoolQuery {
    int value{-1};
    const char* status{"not-run"};
};
struct DriverContext {
    std::uint64_t vehicle{};
    std::int64_t us{};
    const char* reason{"not sampled"};
    bool allowed{};
};
DriverContext driverContext;
std::atomic<std::uint64_t> driverSamples{0},driverAllowed{0},syncCalls{0};
std::atomic<bool> scopeRegistered{false},running{false};
std::atomic<int> requestedMass{1};
std::atomic<bool> includeBikes{true};
std::string gameBuild,loaderBuild,configStatus="not loaded";
bool syncAddressOk{},eventAddressOk{},stackAddressOk{};
std::int64_t resetUs{};

std::atomic<std::uint32_t> protectionFlags{0};
std::string lastFileStatus="not captured";
std::atomic<bool> enabled{true}, installed{false};
std::mutex stateMutex, sampleMutex, reportMutex;
std::vector<std::string> reportPages;
std::filesystem::path configPath;
std::string status="not initialized";
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
// The native JSON is the only configuration authority. Writes are atomic and
// happen outside physics callbacks. A v1 file migrates with vanilla mass.
bool save(bool value,std::uint32_t flags,int multiplier,bool bikes) {
    try {
        if(configPath.empty()||!kineticle::validProtectionFlags(flags)||!kineticle::validMultiplier(multiplier))return false;
        auto tmp=configPath;tmp+=L".tmp";
        {std::ofstream out(tmp,std::ios::binary|std::ios::trunc);
         out<<Json{{"format",2},{"enabled",value},{"massMultiplier",multiplier},{"includeMotorcycles",bikes},{"protectionFlags",flags}}.dump(2)<<"\n";
         out.flush();if(!out)return false;}
        if(!MoveFileExW(tmp.c_str(),configPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return false;
        protectionFlags.store(flags);includeBikes.store(bikes);requestedMass.store(multiplier);enabled.store(value);
        return true;
    }catch(...){return false;}
}
bool load() {
    try {
        auto folder=moduleDirectory();if(folder.empty())return false;configPath=folder/L"config.json";
        if(!std::filesystem::exists(configPath))return save(true,0,1,true);
        std::ifstream in(configPath);auto j=Json::parse(in);in.close();
        if(!j.at("format").is_number_integer())return false;
        const auto format=j.at("format").get<int>();
        if(format!=1&&format!=2)return false;
        const auto& on=j.at(format==1?"probeEnabled":"enabled");
        if(!on.is_boolean())return false;
        auto flags=j.value("protectionFlags",0u);
        if(j.contains("protectionFlags")&&!j.at("protectionFlags").is_number_unsigned())return false;
        if(!kineticle::validProtectionFlags(flags))return false;
        if(format==1)return save(on.get<bool>(),flags,1,true);
        if(!j.at("massMultiplier").is_number_integer()||!j.at("includeMotorcycles").is_boolean())return false;
        const auto mult=j.at("massMultiplier").get<int>();
        if(!kineticle::validMultiplier(mult))return false;
        enabled.store(on.get<bool>());protectionFlags.store(flags);requestedMass.store(mult);
        includeBikes.store(j.at("includeMotorcycles").get<bool>());return true;
    }catch(...){return false;}
}
std::int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void massStep(PhysicsData* rb,void* chassis);
void sync(void* self,void* chassis,PhysicsData* rb,float a4,float dt) {
    massStep(rb,chassis);
    original(self,chassis,rb,a4,dt);
}
// Query reflected zero-argument Bool methods from the script event context.
// Only IsDestroyed explicitly permits a scripted implementation, using the
// SDK's ExecuteFunction path. No RTTI from the PhysX sync callback.
BoolQuery queryBool(RED4ext::vehicle::BaseObject* object,const char* name,bool allowScript=false) {
    if(!object)return {-1,"no-object"};
    auto* type=object->GetType();
    auto* fn=type?type->GetFunction(RED4ext::CName(name)):nullptr;
    if(!fn)return {-1,"missing-method"};
    if(fn->flags.isStatic)return {-1,"static"};
    if(fn->params.size!=0)return {-1,"parameters"};
    if(!fn->returnType||!fn->returnType->type||
       fn->returnType->type->GetName()!=RED4ext::CName("Bool"))return {-1,"return-type"};
    if(!fn->flags.isNative&&!allowScript)return {-1,"scripted-not-invoked"};
    bool result=false;
    if(!RED4ext::ExecuteFunction(static_cast<RED4ext::ScriptInstance>(object),fn,&result))
        return {-1,fn->flags.isNative?"native-execute-failed":"script-execute-failed"};
    return {result?1:0,fn->flags.isNative?"native":"scripted"};
}
bool roadVehicle(RED4ext::vehicle::BaseObject* v,RED4ext::CRTTISystem* rtti) {
    auto* type=v->GetType();
    auto* car=rtti->GetClass("vehicleCarBaseObject");
    auto* bike=rtti->GetClass("vehicleBikeBaseObject");
    return type&&((car&&type->IsA(car))||(includeBikes.load()&&bike&&type->IsA(bike)));
}
// Validate entire span in a committed readable page before examining the
// event argument. This is not a substitute for reflected type validation.
bool readable(const void* ptr,std::size_t size) {
    if(!ptr||!size)return false;
    MEMORY_BASIC_INFORMATION region{};
    if(!VirtualQuery(ptr,&region,sizeof(region))||region.State!=MEM_COMMIT||
       (region.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
       !(region.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|
                         PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
    auto offset=reinterpret_cast<std::uintptr_t>(ptr)-reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    return offset<region.RegionSize&&size<=region.RegionSize-offset;
}
// Record the exact failed gate rather than aggregate all decoder failures.
const RED4ext::Handle<IScriptable>* eventArgument(RED4ext::CBaseFunction* fn,RED4ext::CScriptStack* stack,DecodeSample& d) {
    const auto fail=[&](Reject why)->const RED4ext::Handle<IScriptable>* {d.reason=why;return nullptr;};
    if(!readable(stack,sizeof(RED4ext::CScriptStack)))return fail(Reject::StackMemory);
    if(fn->params.size!=1||!fn->params[0]||!fn->params[0]->type)return fail(Reject::MissingParam);
    auto* param=fn->params[0];
    d.paramSize=param->type->GetSize();d.paramOffset=param->valueOffset;
    d.stackSize=fn->stackSize;d.outParam=param->flags.isOut;d.hasArgs=stack->args!=nullptr;
    if(d.outParam||d.paramSize!=sizeof(RED4ext::Handle<IScriptable>))return fail(Reject::ParamLayout);
    if(!d.hasArgs)return fail(Reject::MissingParam);
    auto* result=stack->GetResultAddr();
    // Event dispatch may discard the callback return; a null result address or
    // type is not evidence of invalid arguments. CET's dispatcher observer
    // uses the result address solely to distinguish the two stack layouts.
    d.hasResult=result!=nullptr;
    d.hasResultType=stack->GetResultType()!=nullptr;
    d.knownVtable=*reinterpret_cast<const std::uintptr_t*>(stack)==stackVtable;
    d.array=stack->value!=result;
    void* value=nullptr;
    if(d.array) {
        if(d.knownVtable&&(!readable(stack,sizeof(RED4ext::CStack))||
            reinterpret_cast<RED4ext::CStack*>(stack)->argsCount!=1))return fail(Reject::ArrayCount);
        if(!readable(stack->args,sizeof(RED4ext::CStackType)))return fail(Reject::ArrayMemory);
        const auto& arg=*reinterpret_cast<const RED4ext::CStackType*>(stack->args);
        d.hasArgType=arg.type!=nullptr;d.argTypeMatches=arg.type==param->type;
        // The SDK ExecuteFunction helper permits initially untyped arguments;
        // the dispatcher observer uses the function's reflected parameter type.
        // Reject conflicting non-null metadata, not absent metadata.
        if(d.hasArgType&&!d.argTypeMatches)return fail(Reject::ArrayType);
        value=arg.value;
    } else {
        if(d.knownVtable)return fail(Reject::PackedLayout);
        // Use RTTI's parameter offset; permit a non-zero offset only when
        // it fits inside the declared function stack. No arbitrary offsets.
        if(d.paramOffset!=0&&(d.stackSize<=0||
            d.paramOffset>static_cast<unsigned>(d.stackSize)||
            d.paramSize>static_cast<unsigned>(d.stackSize)-d.paramOffset))
            return fail(Reject::PackedLayout);
        if(!readable(stack->args,static_cast<std::size_t>(d.paramOffset)+d.paramSize))
            return fail(Reject::ArgumentMemory);
        value=stack->args+d.paramOffset;
    }
    if(!readable(value,sizeof(RED4ext::Handle<IScriptable>))||
       reinterpret_cast<std::uintptr_t>(value)%alignof(RED4ext::Handle<IScriptable>))return fail(Reject::ArgumentMemory);
    return static_cast<const RED4ext::Handle<IScriptable>*>(value);
}
#include "protection.hpp"
void publishDriver(std::uint64_t id,bool allowed,const char* reason);
bool executeScripted(RED4ext::CBaseFunction* fn,RED4ext::CScriptStack* stack,void* a3) {
    // Hook only the exact reflected methods. Reentrant calls made by our
    // scope/RTTI helpers always take the vanilla path.
    if(fn&&enabled.load(std::memory_order_relaxed)&&protectionFlags.load(std::memory_order_relaxed)) {
        static thread_local bool protecting=false;
        if(!protecting&&(fn->fullName==RED4ext::CName("DealDamages;gameHitEvent")||
            fn->fullName==RED4ext::CName("OnGridDestruction"))) {
            protecting=true;
            const bool result=protectDispatch(fn,stack,a3);
            protecting=false;
            return result;
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
#include "mass_experiment.hpp"
void publishDriver(std::uint64_t id,bool allowed,const char* reason){
    std::lock_guard lock(sampleMutex);
    driverContext={id,nowUs(),reason,allowed&&id!=0};
    selectedMassID.store(driverContext.allowed?id:0,std::memory_order_release);
}
bool onRunningEnter(RED4ext::CGameApplication*) {
    running.store(true);publishDriver(0,false,"running entered");return true;
}
bool onRunningUpdate(RED4ext::CGameApplication*) {
    if(!running.load())return true;
    static std::int64_t lastTick=0;
    const auto now=nowUs();if(now-lastTick<100000)return true;lastTick=now;
    if(!enabled.load()||requestedMass.load()<=1||!massReady.load()){
        publishDriver(0,false,"disabled or vanilla");return true;
    }
    ++driverSamples;
    auto player=localPlayer();
    if(!player.GetPtr()){publishDriver(0,false,"player unavailable");return true;}
    H mounted;
    if(!getHandle(player.GetPtr(),"GetMountedVehicle","vehicleBaseObject",mounted)){
        publishDriver(0,false,"not mounted");return true;
    }
    auto* v=static_cast<RED4ext::vehicle::BaseObject*>(mounted.GetPtr());
    const auto reason=scope(v);
    const bool allowed=reason==kineticle::Scope::Allowed;
    if(allowed)++driverAllowed;
    publishDriver(v->entityID.hash,allowed,scopeNames[static_cast<unsigned>(reason)]);
    return true;
}
bool onRunningExit(RED4ext::CGameApplication*){
    running.store(false);publishDriver(0,false,"running exited");return true;
}

using Resolver=std::uintptr_t(*)(std::uint32_t);
void attachEvents(RED4ext::PluginHandle handle,const RED4ext::Sdk* sdk,Resolver resolve) {
    eventTarget=reinterpret_cast<void*>(resolve(RED4ext::Detail::AddressHashes::CBaseFunction_ExecuteScripted));
    stackVtable=resolve(RED4ext::Detail::AddressHashes::CStack_vtbl);
    MEMORY_BASIC_INFORMATION region{};
    eventAddressOk=executableAddress(eventTarget);
    stackAddressOk=stackVtable&&VirtualQuery(reinterpret_cast<void*>(stackVtable),&region,sizeof(region))&&
       region.State==MEM_COMMIT&&region.AllocationBase==GetModuleHandleW(nullptr)&&
       !(region.Protect&(PAGE_GUARD|PAGE_NOACCESS));
    if(!eventAddressOk||!stackAddressOk) {
        eventTarget=nullptr;eventStatus="contact addresses unavailable";return;
    }
    if(!sdk->hooking->Attach(handle,eventTarget,reinterpret_cast<void*>(&executeScripted),
                              reinterpret_cast<void**>(&originalScripted))) {
        eventTarget=nullptr;eventStatus="contact hook attach failed";return;
    }
    eventInstalled.store(true);
    eventStatus="script dispatcher installed; protection";
}
bool attach(RED4ext::PluginHandle handle,const RED4ext::Sdk* sdk) {
    gameBuild=moduleVersion(nullptr);
    auto loader=GetModuleHandleW(L"RED4ext.dll");loaderBuild=loader?moduleVersion(loader):"missing";
    if(gameBuild!="3.0.88.46235"){status="unsupported executable; hook inactive";return false;}
    if(loaderBuild!="1.31.1.0"){status="unsupported RED4ext; hook inactive";return false;}
    if(!sdk||!sdk->hooking||!sdk->hooking->Attach){status="hook API unavailable";return false;}
    // Unlike UniversalRelocBase::Resolve, a missing hash must not terminate the game.
    auto resolve=reinterpret_cast<Resolver>(GetProcAddress(loader,"RED4ext_ResolveAddress"));
    if(!resolve){status="address resolver unavailable";return false;}
    target=reinterpret_cast<void*>(resolve(syncHash));
    syncAddressOk=executableAddress(target);
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
    status="PhysX sync hook installed; native instance mass";
    resolveMass(handle,sdk,resolve);
    attachEvents(handle,sdk,resolve);
    RED4ext::v0::GameState state{onRunningEnter,onRunningUpdate,onRunningExit};
    if(sdk->gameStates&&sdk->gameStates->Add&&sdk->gameStates->Add(handle,RED4ext::EGameStateType::Running,&state)){
        scopeRegistered.store(true);
    }else{massReady.store(false);status="native driver update registration failed; mass disabled";}
    return true;
}
std::string diagnostic() {
    std::lock_guard lock(sampleMutex);
    std::ostringstream s;
    s<<std::setprecision(6)<<std::boolalpha<<version<<"; "<<status<<"; "<<eventStatus
     <<"; ENV game="<<gameBuild<<" RED4ext="<<loaderBuild<<" config="<<configStatus
     <<" sync-address="<<syncAddressOk<<" event-address="<<eventAddressOk<<" stack-address="<<stackAddressOk
     <<"; CONFIG enabled="<<enabled.load()<<" multiplier="<<requestedMass.load()<<" motorcycles="<<includeBikes.load()
     <<"; CORE_SCOPE registered="<<scopeRegistered.load()<<" running="<<running.load()
     <<" samples="<<driverSamples.load()<<" allowed_samples="<<driverAllowed.load()
     <<" vehicle="<<driverContext.vehicle<<" allowed="<<driverContext.allowed
     <<" age_ms="<<(driverContext.us?static_cast<double>(nowUs()-driverContext.us)/1000.0:-1)
     <<" reason="<<driverContext.reason
     <<"; NOT VERIFIED: tire and occupant protection; native mechanical damage callback; collision-only scope vs walls";
    writeProtectionDiagnostics(s);massReport(s);return s.str();
}
void getEnabled(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){frame->code++;if(out)*out=enabled.load();}
void setEnabled(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){
    bool value=false;RED4ext::GetParameter(frame,&value);frame->code++;
    std::lock_guard lock(stateMutex);const auto ok=save(value,protectionFlags.load(),requestedMass.load(),includeBikes.load());
    if(out)*out=ok;
}
void getReport(IScriptable*,CStackFrame* frame,CString* out,std::int64_t){
    frame->code++;if(out)*out=CString(diagnostic().c_str());
}
// Capture once: the controller reads numbered pages from the same snapshot.
// Keep the transport separate from physics and independent of M1 Arc.
// Write a complete report atomically outside physics callbacks. A file is
// also produced on normal plugin unload, so standalone Core needs no controller.
bool writeReportFile(const std::string& snapshot) {
    try {
        auto folder=moduleDirectory();if(folder.empty())return false;
        auto dst=folder/L"diagnostics.txt";auto tmp=folder/L"diagnostics.txt.tmp";
        {std::ofstream f(tmp,std::ios::binary|std::ios::trunc);
         f<<snapshot<<"\n";f.flush();if(!f)return false;}
        return MoveFileExW(tmp.c_str(),dst.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    } catch(...) {return false;}
}
void setMassMultiplier(IScriptable*,CStackFrame* frame,bool* out,std::int64_t) {
    std::int32_t value=1;RED4ext::GetParameter(frame,&value);frame->code++;
    if(out)*out=false;
    if(!kineticle::validMultiplier(value)||(value>1&&(!installed.load()||!massReady.load())))return;
    std::lock_guard lock(stateMutex);
    const bool ok=save(enabled.load(),protectionFlags.load(),value,includeBikes.load());if(out)*out=ok;
}
void getMassMultiplier(IScriptable*,CStackFrame* frame,std::int32_t* out,std::int64_t) {
    frame->code++;if(out)*out=requestedMass.load();
}
void getProtection(IScriptable*,CStackFrame* frame,std::int32_t* out,std::int64_t) {
    frame->code++;if(out)*out=static_cast<std::int32_t>(protectionFlags.load());
}
void setProtection(IScriptable*,CStackFrame* frame,bool* out,std::int64_t) {
    std::int32_t flags=-1;RED4ext::GetParameter(frame,&flags);frame->code++;
    if(out)*out=false;
    if(flags<0||!kineticle::validProtectionFlags(static_cast<std::uint32_t>(flags)))return;
    if(flags&&!eventInstalled.load())return;
    std::lock_guard lock(stateMutex);
    if(out)*out=save(enabled.load(),static_cast<std::uint32_t>(flags),requestedMass.load(),includeBikes.load());
}
void getBikes(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){frame->code++;if(out)*out=includeBikes.load();}
void setBikes(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){
    bool value=false;RED4ext::GetParameter(frame,&value);frame->code++;
    std::lock_guard lock(stateMutex);const bool ok=save(enabled.load(),protectionFlags.load(),requestedMass.load(),value);
    if(out)*out=ok;
}
void getMassAvailable(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){
    frame->code++;if(out)*out=massReady.load()&&scopeRegistered.load();
}
void captureReport(IScriptable*,CStackFrame* frame,std::int32_t* out,std::int64_t) {
    frame->code++;
    if(!out)return;
    auto snapshot=diagnostic();
    std::lock_guard lock(reportMutex);
    const bool saved=writeReportFile(snapshot);
    lastFileStatus=saved?"saved red4ext/plugins/Kineticle/diagnostics.txt":"FAILED to write diagnostics.txt";
    auto pages=kineticle::reportPages(snapshot);
    // Keep M1 Arc transport bounded independently of the full disk report.
    if(pages.size()>30)pages.resize(30);
    pages.insert(pages.begin(),std::string("Full native report: ")+lastFileStatus+
        "; see CORE_SCOPE, MASS and PROTECTION");
    reportPages=std::move(pages);
    *out=static_cast<std::int32_t>(reportPages.size());
}
void getReportPart(IScriptable*,CStackFrame* frame,CString* out,std::int64_t) {
    std::int32_t index=-1;RED4ext::GetParameter(frame,&index);frame->code++;
    if(!out)return;
    std::lock_guard lock(reportMutex);
    if(index<0||static_cast<std::size_t>(index)>=reportPages.size()) {*out=CString("");return;}
    const auto text=kineticle::numberedPage(version,reportPages,static_cast<std::size_t>(index));
    *out=CString(text.c_str());
}
void resetReport(IScriptable*,CStackFrame* frame,bool* out,std::int64_t){
    frame->code++;std::lock_guard lock(sampleMutex);
    driverSamples.store(0);driverAllowed.store(0);resetProtectionDiagnostics();resetMassCounters();resetUs=nowUs();
    if(out)*out=true;
}
void registerTypes(){}
void postRegister(){
    auto* rtti=RED4ext::CRTTISystem::Get();if(!rtti)return;
    const auto add=[&](const char* name,RED4ext::ScriptingFunction_t<void*> callback,const char* result,const char* argType=nullptr,const char* argName=nullptr){
        auto* fn=RED4ext::CGlobalFunction::Create(name,name,callback);
        if(!fn)return;
        fn->flags=RED4ext::CBaseFunction::Flags{.isNative=true,.isStatic=true};
        fn->SetReturnType(result);if(argType)fn->AddParam(argType,argName);
        rtti->RegisterFunction(fn);
    };
    add("Kineticle_GetProbeEnabled",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getEnabled),"Bool");
    add("Kineticle_SetProbeEnabled",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&setEnabled),"Bool","Bool","enabled");
    add("Kineticle_Diagnostics",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getReport),"String");
    add("Kineticle_ResetDiagnostics",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&resetReport),"Bool");
    add("Kineticle_GetMassMultiplier",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getMassMultiplier),"Int32");
    add("Kineticle_SetMassMultiplier",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&setMassMultiplier),"Bool","Int32","multiplier");
    add("Kineticle_GetProtectionFlags",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getProtection),"Int32");
    add("Kineticle_SetProtectionFlags",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&setProtection),"Bool","Int32","flags");
    add("Kineticle_CaptureDiagnostics",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&captureReport),"Int32");
    add("Kineticle_DiagnosticPart",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getReportPart),"String","Int32","index");
    add("Kineticle_GetIncludeMotorcycles",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getBikes),"Bool");
    add("Kineticle_SetIncludeMotorcycles",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&setBikes),"Bool","Bool","enabled");
    add("Kineticle_GetMassAvailable",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getMassAvailable),"Bool");
}
}
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle handle,RED4ext::EMainReason reason,const RED4ext::Sdk* sdk){
    if(reason==RED4ext::EMainReason::Unload){
        enabled.store(false);running.store(false);publishDriver(0,false,"unloading");
        if(eventInstalled.load()&&sdk&&sdk->hooking&&sdk->hooking->Detach){
            sdk->hooking->Detach(handle,eventTarget);eventInstalled.store(false);
        }
        if(installed.load()&&sdk&&sdk->hooking&&sdk->hooking->Detach){
            sdk->hooking->Detach(handle,target);installed.store(false);
        }
        if(massWriterTarget&&sdk&&sdk->hooking&&sdk->hooking->Detach) {sdk->hooking->Detach(handle,massWriterTarget);massWriterTarget=nullptr;}
        // Preserve the last manual snapshot. On standalone normal exit, or
        // if no manual report succeeded, save the current native observations.
        if(lastFileStatus.rfind("saved",0)!=0)writeReportFile(diagnostic());
        return true;
    }
    if(reason!=RED4ext::EMainReason::Load)return true;
    auto* rtti=RED4ext::CRTTISystem::Get();if(!rtti)return false;
    rtti->AddRegisterCallback(registerTypes);rtti->AddPostRegisterCallback(postRegister);
    resetUs=nowUs();
    if(load()) {configStatus="ready";attach(handle,sdk);}
    else {configStatus="invalid or inaccessible";enabled.store(false);status="config invalid or inaccessible; hook inactive";}
    if(sdk&&sdk->logger&&sdk->logger->Info){const auto message=diagnostic();sdk->logger->Info(handle,message.c_str());}
    return true;
}
RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::PluginInfo* info){
    info->name=L"Kineticle";info->author=L"MISO";info->version=RED4EXT_SEMVER(0,1,0);
    info->runtime=RED4EXT_RUNTIME_2_50;info->sdk=RED4EXT_SDK_LATEST;
}
RED4EXT_C_EXPORT std::uint32_t RED4EXT_CALL Supports(){return RED4EXT_API_VERSION_LATEST;}
