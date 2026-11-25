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
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include "physics.hpp"
#include "contact_trace.hpp"
#include <chrono>

namespace {
using RED4ext::IScriptable;
using RED4ext::CStackFrame;
using RED4ext::CString;
using PhysicsData=RED4ext::vehicle::PhysicsData;
using Json=nlohmann::json;
constexpr const char* version="0.1.0-dev.6-native-probe.5";
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
std::atomic<std::uint64_t> bumpCalls{0}, rejectedStacks{0}, invalidPairs{0},
    observedPairs{0}, npcPairs{0}, driverQueryFailures{0}, playerPairs{0},
    flagDisagreements{0}, guardedPairs{0}, arrayStacks{0}, packedStacks{0};
std::string eventStatus="contact hook inactive";
enum class Reject : unsigned { None, StackMemory, MissingParam, ParamLayout, ArrayCount,
    ArrayMemory, ArrayType, PackedLayout, ArgumentMemory, EventObject, Count };
constexpr const char* rejectNames[]={"none","stack-memory","missing-param","param-layout",
    "array-count","array-memory","array-type","packed-layout","argument-memory","event-object"};
static_assert(std::size(rejectNames)==static_cast<unsigned>(Reject::Count));
std::atomic<std::uint64_t> rejectCounts[static_cast<unsigned>(Reject::Count)]{};
struct DecodeSample {
    Reject reason{Reject::None};
    bool array{},knownVtable{},hasResult{},hasResultType{},hasArgType{},argTypeMatches{},hasArgs{};
    bool outParam{};
    std::uint32_t paramSize{},paramOffset{};
    int stackSize{};
};
DecodeSample lastDecode;
struct PairSample {
    std::uint64_t receiver{},other{},syncSequence{};
    int receiverDriver{-1},otherDriver{-1},remote{-1},quest{-1},destroyed{-1},otherDestroyed{-1},otherQuest{-1};
    bool destroyedScripted{},otherDestroyedScripted{};
    bool receiverCandidate{},otherCandidate{},roadVehicle{};
    float delta{};
    kineticle::Vec normal{};
    DWORD thread{};
};
PairSample lastPair;
kineticle::ContactTrace trace;
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
std::int64_t nowUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void sync(void* self,void* chassis,PhysicsData* rb,float a4,float dt) {
    if (!enabled.load(std::memory_order_relaxed)||!rb||!chassis) { original(self,chassis,rb,a4,dt);return; }
    const auto sequence=calls.fetch_add(1,std::memory_order_relaxed)+1;
    const auto* owner=rb->vehicle;
    if(!owner||owner->physicsData!=rb) {original(self,chassis,rb,a4,dt);return;}
    const auto id=owner->entityID.hash;
    const bool candidate=owner->isPlayerControlled!=0;
    const auto before=vec(rb->velocity),angularBefore=vec(rb->angularVelocity);
    original(self,chassis,rb,a4,dt);
    if(candidate)candidateFrames.fetch_add(1,std::memory_order_relaxed);
    const auto after=vec(rb->velocity),angularAfter=vec(rb->angularVelocity);
    if(!id||!kineticle::valid(before)||!kineticle::valid(after)||
       !kineticle::valid(angularBefore)||!kineticle::valid(angularAfter))return;
    const auto loss=kineticle::forwardLoss(before,after);
    const auto change=kineticle::length(kineticle::sub(after,before));
    // Observe meaningful sync corrections on BOTH vehicles. No RTTI or stored
    // entity pointers, and no modification of either rigid body.
    if(change<0.1f&&(!candidate||loss<0.5f))return;
    const auto stamp=nowUs();
    std::lock_guard lock(sampleMutex);
    trace.push({id,sequence,stamp,GetCurrentThreadId(),before,after,angularBefore,angularAfter,loss,change});
    if(candidate&&loss>=0.5f) {
        last={before,after,angularBefore,angularAfter,loss,sequence};
        corrections.fetch_add(1,std::memory_order_relaxed);
    }
}
// Query reflected zero-argument Bool methods from the script event context.
// Only IsDestroyed explicitly permits a scripted implementation, using the
// SDK's ExecuteFunction path. No RTTI from the PhysX sync callback.
int readNativeBool(RED4ext::vehicle::BaseObject* object,const char* name,bool* scripted=nullptr) {
    auto* type=object->GetType();
    auto* fn=type?type->GetFunction(RED4ext::CName(name)):nullptr;
    if(!fn||(!fn->flags.isNative&&!scripted)||fn->flags.isStatic||fn->params.size!=0||
       !fn->returnType||!fn->returnType->type||
       fn->returnType->type->GetName()!=RED4ext::CName("Bool")) return -1;
    if(scripted)*scripted=!fn->flags.isNative;
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
void recordDecode(DecodeSample sample) {
    if(sample.reason!=Reject::None) {
        rejectCounts[static_cast<unsigned>(sample.reason)].fetch_add(1,std::memory_order_relaxed);
        rejectedStacks.fetch_add(1,std::memory_order_relaxed);
    }
    std::lock_guard lock(sampleMutex);lastDecode=sample;
}
const RED4ext::Handle<IScriptable>* bumpArgument(RED4ext::CBaseFunction* fn,RED4ext::CScriptStack* stack,DecodeSample& d) {
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
void observeBump(RED4ext::CBaseFunction* fn,RED4ext::CScriptStack* stack) {
    bumpCalls.fetch_add(1,std::memory_order_relaxed);
    if(!readable(stack,sizeof(RED4ext::CScriptStack))) {
        DecodeSample d;d.reason=Reject::StackMemory;recordDecode(d);return;
    }
    auto* rtti=RED4ext::CRTTISystem::Get();
    auto* vehicleType=rtti?rtti->GetClass("vehicleBaseObject"):nullptr;
    auto* bumpType=rtti?rtti->GetClass("vehicleVehicleBumpEvent"):nullptr;
    auto* context=stack->GetContext();
    if(!vehicleType||!bumpType||!context||!context->GetType()||
       !context->GetType()->IsA(vehicleType)||fn->params.size!=1||!fn->params[0]) {
        invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    auto* paramType=fn->params[0]->type;
    if(!paramType||paramType->GetType()!=RED4ext::ERTTIType::Handle||
       static_cast<RED4ext::CRTTIHandleType*>(paramType)->GetInnerType()!=bumpType) {
        invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    DecodeSample decode;
    const auto* eventHandle=bumpArgument(fn,stack,decode);
    if(!eventHandle){recordDecode(decode);return;}
    auto* event=eventHandle->GetPtr();
    if(!readable(event,sizeof(RED4ext::vehicle::VehicleBumpEvent))||
       !event->GetType()||!event->GetType()->IsA(bumpType)) {
        decode.reason=Reject::EventObject;recordDecode(decode);return;
    }
    (decode.array?arrayStacks:packedStacks).fetch_add(1,std::memory_order_relaxed);
    recordDecode(decode);
    auto* bump=static_cast<RED4ext::vehicle::VehicleBumpEvent*>(event);
    auto otherHandle=bump->hitVehicle.Lock();
    auto* other=otherHandle.GetPtr();
    auto* receiver=static_cast<RED4ext::vehicle::BaseObject*>(context);
    if(!other||other==receiver||!other->GetType()||!other->GetType()->IsA(vehicleType)||
       !receiver->entityID.hash||!other->entityID.hash||receiver->entityID.hash==other->entityID.hash||
       !std::isfinite(bump->impactVelocityChange)||!kineticle::valid(vec(bump->hitNormal))) {
        invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    const auto eventTime=nowUs();
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
        sample.destroyed=readNativeBool(driver,"IsDestroyed",&sample.destroyedScripted);
        auto* struck=driver==receiver?other:receiver;
        sample.otherDestroyed=readNativeBool(struck,"IsDestroyed",&sample.otherDestroyedScripted);
        sample.otherQuest=readNativeBool(struck,"IsInQuestOrScenePhysicsState");
        sample.roadVehicle=roadVehicle(driver,rtti);
        if(sample.remote==0&&sample.quest==0&&sample.destroyed==0&&sample.otherDestroyed==0&&sample.otherQuest==0&&sample.roadVehicle)
            guardedPairs.fetch_add(1,std::memory_order_relaxed);
    }
    // Store only numeric observations, never keep a vehicle pointer across callbacks.
    std::lock_guard lock(sampleMutex);lastPair=sample;
    if((sample.receiverDriver==1&&sample.otherDriver==0)||(sample.otherDriver==1&&sample.receiverDriver==0)) {
        const auto driverId=sample.receiverDriver==1?sample.receiver:sample.other;
        const auto targetId=sample.receiverDriver==1?sample.other:sample.receiver;
        trace.event(driverId,targetId,eventTime,sample.syncSequence,sample.delta);
    }
}
bool executeScripted(RED4ext::CBaseFunction* fn,RED4ext::CScriptStack* stack,void* a3) {
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
     <<"; rejected stacks="<<rejectedStacks.load()<<"; array stacks="<<arrayStacks.load()
     <<"; packed stacks="<<packedStacks.load()<<"; last decoder="<<rejectNames[static_cast<unsigned>(lastDecode.reason)]
     <<"; decoder array="<<lastDecode.array<<"; known vtable="<<lastDecode.knownVtable
     <<"; result address present="<<lastDecode.hasResult<<"; result type present="<<lastDecode.hasResultType
     <<"; args present="<<lastDecode.hasArgs<<"; array type present="<<lastDecode.hasArgType
     <<"; array type matches="<<lastDecode.argTypeMatches<<"; param size="<<lastDecode.paramSize
     <<"; param offset="<<lastDecode.paramOffset<<"; param out="<<lastDecode.outParam
     <<"; function stack size="<<lastDecode.stackSize<<"; invalid pairs="<<invalidPairs.load()
     <<"; observed vehicle pairs="<<observedPairs.load()<<"; NPC pairs="<<npcPairs.load()
     <<"; driver query failures="<<driverQueryFailures.load()
     <<"; player driver pairs="<<playerPairs.load()<<"; candidate flag disagreements="<<flagDisagreements.load()
     <<"; pairs passing native guards="<<guardedPairs.load()
     <<"; last pair receiver="<<lastPair.receiver<<"; other="<<lastPair.other
     <<"; receiver candidate="<<lastPair.receiverCandidate<<"; other candidate="<<lastPair.otherCandidate
     <<"; receiver IsPlayerDriver="<<lastPair.receiverDriver<<"; other IsPlayerDriver="<<lastPair.otherDriver
     <<"; driver remote="<<lastPair.remote<<"; quest="<<lastPair.quest<<"; destroyed="<<lastPair.destroyed
     <<"; destroyed scripted="<<lastPair.destroyedScripted<<"; target destroyed="<<lastPair.otherDestroyed
     <<"; target destroyed scripted="<<lastPair.otherDestroyedScripted<<"; target quest="<<lastPair.otherQuest
     <<"; driver car/bike="<<lastPair.roadVehicle<<"; impact delta="<<lastPair.delta
     <<"; hit normal=("<<lastPair.normal.x<<","<<lastPair.normal.y<<","<<lastPair.normal.z<<")"
     <<"; event sync sequence="<<lastPair.syncSequence<<"; event thread="<<lastPair.thread
     <<"; PSM and exact contact-to-sync correlation unverified; all gameplay writes inactive";
    s<<"; decoder rejections by reason=";
    bool any=false;
    for(unsigned i=1;i<static_cast<unsigned>(Reject::Count);++i) {
        auto count=rejectCounts[i].load();
        if(count){if(any)s<<",";s<<rejectNames[i]<<":"<<count;any=true;}
    }
    if(!any)s<<"none";
    // Each trace includes strongest observed correction before/after the
    // event for each exact entity ID. Temporal proximity is NOT causation.
    auto outputMatch=[&](const char* label,const kineticle::TraceSample& m,std::int64_t t) {
        s<<" "<<label<<"=";
        if(!m.id){s<<"none";return;}
        s<<"{dt_ms:"<<static_cast<double>(m.us-t)/1000.0<<",seq:"<<m.sequence
         <<",thread:"<<m.thread<<",dv:"<<m.change<<",loss:"<<m.loss
         <<",before:("<<m.before.x<<","<<m.before.y<<","<<m.before.z<<")"
         <<",after:("<<m.after.x<<","<<m.after.y<<","<<m.after.z<<")"
         <<",angular_dv:"<<kineticle::length(kineticle::sub(m.angularAfter,m.angularBefore))<<"}";
    };
    for(const auto& e:trace.events) {
        if(!e.driver)continue;
        s<<"; contact trace driver="<<e.driver<<" target="<<e.target<<" event_seq="<<e.sequence<<" bump_dv="<<e.delta;
        outputMatch("V-pre",e.driverPre,e.us);outputMatch("V-post",e.driverPost,e.us);
        outputMatch("NPC-pre",e.targetPre,e.us);outputMatch("NPC-post",e.targetPost,e.us);
    }
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
    guardedPairs.store(0);arrayStacks.store(0);packedStacks.store(0);lastPair={};lastDecode={};trace={};
    for(auto& counter:rejectCounts)counter.store(0);
    if(out)*out=true;
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
