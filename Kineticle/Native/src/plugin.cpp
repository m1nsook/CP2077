// Kineticle native development: opt-in paired horizontal motion experiment.
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
#include "report_pages.hpp"
#include "diagnostic_policy.hpp"
#include "motion_trial.hpp"
#include <iomanip>
#include <chrono>

namespace {
using RED4ext::IScriptable;
using RED4ext::CStackFrame;
using RED4ext::CString;
using PhysicsData=RED4ext::vehicle::PhysicsData;
using Json=nlohmann::json;
constexpr const char* version="0.1.0-dev.8-native-motion";
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
// Per-entity checks are independent. -1 means unknown, never a pass.
struct BoolQuery {
    int value{-1};
    const char* status{"not-run"};
};
struct VehicleGuard {
    BoolQuery driver,remote,quest,destroyed;
    bool road{},candidate{};
    std::uint32_t physicsState{};
};
struct ControllerContext {
    std::uint64_t vehicle{};
    std::int64_t us{};
    std::string detail{"unavailable (optional controller not sampled)"};
    bool allowed{};
};
ControllerContext controllerContext;
struct PairSample {
    std::uint64_t receiver{},other{},syncSequence{};
    int receiverDriver{-1},otherDriver{-1},remote{-1},quest{-1},destroyed{-1},otherDestroyed{-1},otherQuest{-1};
    bool destroyedScripted{},otherDestroyedScripted{};
    bool receiverCandidate{},otherCandidate{},roadVehicle{};
    float delta{};
    kineticle::Vec normal{};
    DWORD thread{};
    VehicleGuard receiverChecks,otherChecks;
    std::int64_t eventUs{};
    bool traffic{},duplicate{};
    std::int64_t contextAgeUs{-1};
    bool contextMatches{};
    std::string context{"unavailable"};
};
PairSample lastPair;
std::array<PairSample,8> pairHistory{};
std::size_t pairIndex{};
std::atomic<std::uint64_t> invalidOwner{0},invalidVectors{0},invalidMass{0},invalidDt{0},
    syncObserved{0},traceSamples{0},reentrantEvents{0},duplicateEvents{0},
    absentOther{0},badContext{0},badSignature{0},badPair{0};
std::atomic<std::uint64_t> syncCostUs{0},syncCostMaxUs{0},eventCostUs{0},eventCostMaxUs{0};
std::string gameBuild,loaderBuild,configStatus="not loaded";
bool syncAddressOk{},eventAddressOk{},stackAddressOk{},impulseAddressOk{},angularAddressOk{};
std::int64_t resetUs{};

kineticle::ContactTrace trace;
kineticle::MotionTrial trial;
std::atomic<bool> experiment{false};
std::atomic<std::uint64_t> trialDriver{0},trialTarget{0};
std::string lastFileStatus="not captured";
std::atomic<bool> enabled{true}, installed{false};
std::atomic<std::uint64_t> calls{0}, candidateFrames{0}, corrections{0};
std::mutex stateMutex, sampleMutex, reportMutex;
std::vector<std::string> reportPages;
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
void addCost(std::atomic<std::uint64_t>& total,std::atomic<std::uint64_t>& maximum,std::int64_t start) {
    const auto elapsed=static_cast<std::uint64_t>(std::max<std::int64_t>(0,nowUs()-start));
    total.fetch_add(elapsed,std::memory_order_relaxed);
    auto old=maximum.load(std::memory_order_relaxed);
    while(old<elapsed&&!maximum.compare_exchange_weak(old,elapsed,std::memory_order_relaxed)) {}
}
bool contextAllows(std::uint64_t id,std::int64_t stamp) {
    return id&&controllerContext.vehicle==id&&controllerContext.allowed&&
        stamp>=controllerContext.us&&stamp-controllerContext.us<=750000;
}
// All trial state is serialized by sampleMutex. The atomic IDs are only a
// fast-path filter: they never authorize a write or retain a body pointer.
void publishTrialIds() {
    trialDriver.store(trial.active?trial.driver:0,std::memory_order_relaxed);
    trialTarget.store(trial.active?trial.target:0,std::memory_order_relaxed);
}
void sync(void* self,void* chassis,PhysicsData* rb,float a4,float dt) {
    if (!enabled.load(std::memory_order_relaxed)||!rb||!chassis) { original(self,chassis,rb,a4,dt);return; }
    const auto sequence=calls.fetch_add(1,std::memory_order_relaxed)+1;
    const auto* owner=rb->vehicle;
    if(!owner||owner->physicsData!=rb) {invalidOwner.fetch_add(1,std::memory_order_relaxed);original(self,chassis,rb,a4,dt);return;}
    const auto id=owner->entityID.hash;
    const bool candidate=owner->isPlayerControlled!=0;
    const auto before=vec(rb->velocity),angularBefore=vec(rb->angularVelocity);
    original(self,chassis,rb,a4,dt);
    const auto start=nowUs();
    syncObserved.fetch_add(1,std::memory_order_relaxed);
    if(candidate)candidateFrames.fetch_add(1,std::memory_order_relaxed);
    const auto after=vec(rb->velocity),angularAfter=vec(rb->angularVelocity);
    if(!id||!kineticle::valid(before)||!kineticle::valid(after)||
       !kineticle::valid(angularBefore)||!kineticle::valid(angularAfter)) {invalidVectors.fetch_add(1,std::memory_order_relaxed);return;}
    const auto loss=kineticle::forwardLoss(before,after);
    const auto change=kineticle::length(kineticle::sub(after,before));
    const auto mass=rb->total_mass, inverse=rb->inverseMass;
    const auto massOk=kineticle::validMass(mass,inverse);
    if(!massOk)invalidMass.fetch_add(1,std::memory_order_relaxed);
    if(!std::isfinite(dt)||dt<=0||dt>1)invalidDt.fetch_add(1,std::memory_order_relaxed);
    const bool meaningful=change>=0.1f||(candidate&&loss>=0.5f);
    const bool trialCandidate=experiment.load(std::memory_order_relaxed)&&
        (id==trialDriver.load(std::memory_order_relaxed)||id==trialTarget.load(std::memory_order_relaxed));
    if(!meaningful&&!trialCandidate) {addCost(syncCostUs,syncCostMaxUs,start);return;}
    const auto stamp=nowUs();
    const kineticle::TraceSample current{id,sequence,stamp,GetCurrentThreadId(),before,after,angularBefore,angularAfter,loss,change,
        mass,inverse,dt,static_cast<std::uint32_t>(owner->physicsState),candidate,
        vec(rb->force),vec(rb->torque),massOk};
    std::lock_guard lock(sampleMutex);
    // Preserve vanilla observations before any optional adjustment.
    if(meaningful) {
        trace.push(current);traceSamples.fetch_add(1,std::memory_order_relaxed);
    }
    if(trialCandidate&&experiment.load(std::memory_order_relaxed)&&enabled.load(std::memory_order_relaxed)&&trial.active) {
        // The planner returns at most one adjustment for THIS callback's body.
        // Never wait for another physics worker or touch its rigid body.
        auto action=trial.step(current,contextAllows(trial.driver,stamp));
        if(action.apply) {
            auto result=kineticle::add(after,action.delta);
            rb->velocity=RED4ext::Vector3(result.x,result.y,result.z);
            trial.recordWrite(action,after,vec(rb->velocity),stamp);
        }
        publishTrialIds();
    }
    if(candidate&&loss>=0.5f) {
        last={before,after,angularBefore,angularAfter,loss,sequence};
        corrections.fetch_add(1,std::memory_order_relaxed);
    }
    addCost(syncCostUs,syncCostMaxUs,start);
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
    return type&&((car&&type->IsA(car))||(bike&&type->IsA(bike)));
}
VehicleGuard vehicleChecks(RED4ext::vehicle::BaseObject* v,RED4ext::CRTTISystem* rtti) {
    VehicleGuard g;
    g.driver=queryBool(v,"IsPlayerDriver");
    g.remote=queryBool(v,"IsVehicleRemoteControlled");
    g.quest=queryBool(v,"IsInQuestOrScenePhysicsState");
    g.destroyed=queryBool(v,"IsDestroyed",true);
    g.candidate=v->isPlayerControlled!=0;g.road=roadVehicle(v,rtti);
    g.physicsState=static_cast<std::uint32_t>(v->physicsState);
    return g;
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
        badContext.fetch_add(1,std::memory_order_relaxed);invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
    }
    auto* paramType=fn->params[0]->type;
    if(!paramType||paramType->GetType()!=RED4ext::ERTTIType::Handle||
       static_cast<RED4ext::CRTTIHandleType*>(paramType)->GetInnerType()!=bumpType) {
        badSignature.fetch_add(1,std::memory_order_relaxed);invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
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
    if(!other) {absentOther.fetch_add(1,std::memory_order_relaxed);invalidPairs.fetch_add(1,std::memory_order_relaxed);return;}
    if(other==receiver||!other->GetType()||!other->GetType()->IsA(vehicleType)||
       !receiver->entityID.hash||!other->entityID.hash||receiver->entityID.hash==other->entityID.hash||
       !std::isfinite(bump->impactVelocityChange)||!kineticle::valid(vec(bump->hitNormal))) {
        badPair.fetch_add(1,std::memory_order_relaxed);invalidPairs.fetch_add(1,std::memory_order_relaxed);return;
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
    sample.receiverChecks=vehicleChecks(receiver,rtti);
    sample.otherChecks=vehicleChecks(other,rtti);
    sample.receiverDriver=sample.receiverChecks.driver.value;
    sample.otherDriver=sample.otherChecks.driver.value;
    sample.eventUs=eventTime;sample.traffic=bump->isInTraffic;
    if(sample.receiverDriver<0||sample.otherDriver<0)driverQueryFailures.fetch_add(1,std::memory_order_relaxed);
    if((sample.receiverDriver>=0&&sample.receiverCandidate!=(sample.receiverDriver==1))||
       (sample.otherDriver>=0&&sample.otherCandidate!=(sample.otherDriver==1)))flagDisagreements.fetch_add(1,std::memory_order_relaxed);
    const bool playerPair=kineticle::oneDriver(sample.receiverDriver,sample.otherDriver);
    const bool npcPair=sample.receiverDriver==0&&sample.otherDriver==0&&!sample.receiverCandidate&&!sample.otherCandidate;
    if(npcPair)npcPairs.fetch_add(1,std::memory_order_relaxed);
    std::uint64_t driverId=0,targetId=0;
    bool trialGuards=false;
    if(playerPair) {
        playerPairs.fetch_add(1,std::memory_order_relaxed);
        const auto& driver=sample.receiverDriver==1?sample.receiverChecks:sample.otherChecks;
        const auto& struck=sample.receiverDriver==1?sample.otherChecks:sample.receiverChecks;
        driverId=sample.receiverDriver==1?sample.receiver:sample.other;
        targetId=sample.receiverDriver==1?sample.other:sample.receiver;
        sample.remote=driver.remote.value;sample.quest=driver.quest.value;
        sample.destroyed=driver.destroyed.value;sample.otherDestroyed=struck.destroyed.value;
        sample.otherQuest=struck.quest.value;sample.roadVehicle=driver.road;
        sample.destroyedScripted=std::string_view(driver.destroyed.status)=="scripted";
        sample.otherDestroyedScripted=std::string_view(struck.destroyed.status)=="scripted";
        const bool guards=kineticle::nativeGuards(driver.driver.value,struck.driver.value,driver.remote.value,
            driver.quest.value,driver.destroyed.value,struck.destroyed.value,struck.quest.value,
            driver.road,driver.candidate,struck.candidate);
        if(guards)guardedPairs.fetch_add(1,std::memory_order_relaxed);
        // Traffic may transition to a dynamic chassis before sync. Permit
        // that one event-state bit, but require both bodies fully dynamic at
        // sample and write time. Reject every other state (including scene).
        trialGuards=guards&&struck.road&&struck.remote.value==0&&driver.physicsState==0&&
            (struck.physicsState==0||struck.physicsState==1);
    }
    std::lock_guard lock(sampleMutex);
    // Nearby repeated callbacks are labeled, not discarded: two distinct
    // contacts may happen close together and no exact contact ID is known.
    for(const auto& old:pairHistory) {
        if(kineticle::nearPair(sample.receiver,sample.other,eventTime,old.receiver,old.other,old.eventUs)) {
            sample.duplicate=true;break;
        }
    }
    if(sample.duplicate)duplicateEvents.fetch_add(1,std::memory_order_relaxed);
    if(controllerContext.us) {
        sample.contextAgeUs=eventTime-controllerContext.us;
        sample.contextMatches=driverId&&driverId==controllerContext.vehicle&&
            sample.contextAgeUs>=0&&sample.contextAgeUs<=750000;
        sample.context=controllerContext.detail;
    }
    pairHistory[pairIndex++%pairHistory.size()]=sample;
    if(!npcPair)lastPair=sample;
    if(playerPair)trace.event(driverId,targetId,eventTime,sample.syncSequence,sample.delta);
    if(experiment.load(std::memory_order_relaxed)&&enabled.load(std::memory_order_relaxed)) {
        if(playerPair) {
            // Any additional event while a transaction is pending makes the
            // association ambiguous. Cancel rather than reuse old samples.
            if(trial.active)trial.cancel(kineticle::TrialReject::Overlap);
            else if(!trialGuards)trial.reject(kineticle::TrialReject::Guards);
            else if(!sample.contextMatches||!contextAllows(driverId,nowUs()))
                trial.reject(kineticle::TrialReject::Context);
            else if(sample.duplicate||!std::isfinite(sample.delta)||sample.delta<0.10f)
                trial.reject(kineticle::TrialReject::Event);
            else {
                trial.arm(driverId,targetId,eventTime,sample.syncSequence);
                // Sync jobs can finish while RTTI queries are running.
                // Seed from the earliest post-event samples in the bounded ring.
                trial.seed(trace.samples);
                trial.expire(nowUs());
            }
        } else if(trial.active&&(sample.receiver==trial.driver||sample.other==trial.driver||
            sample.receiver==trial.target||sample.other==trial.target))trial.cancel(kineticle::TrialReject::Overlap);
        publishTrialIds();
    }

}
bool executeScripted(RED4ext::CBaseFunction* fn,RED4ext::CScriptStack* stack,void* a3) {
    if(enabled.load(std::memory_order_relaxed)&&fn&&
       fn->shortName==RED4ext::CName("OnVehicleBumpEvent")) {
        // A native Bool query should not reenter this event, but fail closed if it does.
        static thread_local bool observing=false;
        if(!observing) {
            observing=true;
            const auto start=nowUs();
            observeBump(fn,stack);
            addCost(eventCostUs,eventCostMaxUs,start);
            observing=false;
        } else reentrantEvents.fetch_add(1,std::memory_order_relaxed);
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
    eventStatus="read-only scripted event dispatcher hook installed; coverage unverified";
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
    impulseAddressOk=executableAddress(reinterpret_cast<void*>(resolve(611586815u)));
    angularAddressOk=executableAddress(reinterpret_cast<void*>(resolve(3303544265u)));
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
    status="PhysX sync hook installed; experimental writes require explicit session opt-in";
    attachEvents(handle,sdk,resolve);
    return true;
}
std::string diagnostic() {
    std::lock_guard lock(sampleMutex);
    std::ostringstream s;
    s<<std::setprecision(6)<<std::boolalpha;
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
     <<"; PSM sampled; exact contact ID unverified; experimental_enabled="<<experiment.load();
    s<<"; decoder rejections by reason=";
    bool any=false;
    for(unsigned i=1;i<static_cast<unsigned>(Reject::Count);++i) {
        auto count=rejectCounts[i].load();
        if(count){if(any)s<<",";s<<rejectNames[i]<<":"<<count;any=true;}
    }
    if(!any)s<<"none";
    s<<"; ENV game="<<gameBuild<<" RED4ext="<<loaderBuild<<" config="<<configStatus
     <<" sync-address="<<syncAddressOk<<" event-address="<<eventAddressOk<<" stack-address="<<stackAddressOk
     <<" impulse-address="<<impulseAddressOk<<" angular-impulse-address="<<angularAddressOk
     <<" (address checks only; no impulse invoked)"
     <<"; PERF elapsed_ms="<<(resetUs?static_cast<double>(nowUs()-resetUs)/1000.0:0)
     <<" valid_sync_owner="<<syncObserved.load()<<" trace_samples="<<traceSamples.load()
     <<" post_sync_total_us="<<syncCostUs.load()<<" post_sync_max_us="<<syncCostMaxUs.load()
     <<" event_total_us="<<eventCostUs.load()<<" event_max_us="<<eventCostMaxUs.load()
     <<" trace_storage_bytes="<<sizeof(trace)<<" pair_storage_bytes="<<sizeof(pairHistory)
     <<"; REJECTIONS owner="<<invalidOwner.load()<<" vectors="<<invalidVectors.load()
     <<" mass="<<invalidMass.load()<<" dt="<<invalidDt.load()<<" reentrant="<<reentrantEvents.load()
     <<" missing_other="<<absentOther.load()<<" context="<<badContext.load()
     <<" signature="<<badSignature.load()<<" pair="<<badPair.load()
     <<" near_duplicate="<<duplicateEvents.load()
     <<"; CONTROLLER_CONTEXT vehicle="<<controllerContext.vehicle<<" age_ms="
     <<(controllerContext.us?static_cast<double>(nowUs()-controllerContext.us)/1000.0:-1)
     <<" "<<controllerContext.detail
     <<"; NOT VERIFIED: exact contact ID; standalone native PSM/tags; environment coverage; damage/visual/tire hooks; native impulse ABI; experimental delivery in game";
    s<<"; MOTION_TRIAL enabled="<<experiment.load()<<" armed="<<trial.armed
     <<" plans="<<trial.planned<<" NPC_writes="<<trial.targetWrites<<" V_writes="<<trial.driverWrites
     <<" completed="<<trial.completed<<" partial="<<trial.partial<<" pending="<<trial.active
     <<" last_reason="<<kineticle::trialReason(trial.lastReason)
     <<" last_driver="<<trial.driver<<" last_target="<<trial.target
     <<" last_pair_error="<<trial.plan.error<<" last_driver_add=("<<trial.plan.driverAdd.x<<","<<trial.plan.driverAdd.y<<","<<trial.plan.driverAdd.z<<")"
     <<" last_target_add=("<<trial.plan.targetAdd.x<<","<<trial.plan.targetAdd.y<<","<<trial.plan.targetAdd.z<<")";
    for(unsigned i=1;i<static_cast<unsigned>(kineticle::TrialReject::Count);++i)
        s<<" "<<kineticle::trialReason(static_cast<kineticle::TrialReject>(i))<<"="<<trial.rejections[i];
    for(std::size_t i=0;i<std::min(trial.writeIndex,trial.writes.size());++i) {
        const auto& w=trial.writes[(trial.writeIndex-1-i)%trial.writes.size()];
        s<<"; MOTION_WRITE role="<<(w.driver?"V":"NPC")<<" id="<<w.id<<" pair_event_us="<<w.eventUs
         <<" offset_ms="<<static_cast<double>(w.us-w.eventUs)/1000.0
         <<" before=("<<w.before.x<<","<<w.before.y<<","<<w.before.z<<")"
         <<" after=("<<w.after.x<<","<<w.after.y<<","<<w.after.z<<")";
    }
    auto writeGuard=[&](const char* label,const VehicleGuard& g) {
        auto q=[&](const char* n,const BoolQuery& v){s<<" "<<n<<"="<<v.value<<"("<<v.status<<")";};
        s<<" "<<label<<":";
        q("driver",g.driver);q("remote",g.remote);q("quest",g.quest);q("destroyed",g.destroyed);
        s<<" road="<<g.road<<" candidate="<<g.candidate<<" physics_state_mask="<<g.physicsState;
    };
    for(std::size_t i=0;i<std::min(pairIndex,pairHistory.size());++i) {
        const auto& e=pairHistory[(pairIndex-1-i)%pairHistory.size()];
        s<<"; PAIR #"<<pairIndex-i<<" receiver="<<e.receiver<<" other="<<e.other<<" delta="<<e.delta
         <<" normal=("<<e.normal.x<<","<<e.normal.y<<","<<e.normal.z<<")"<<" traffic="<<e.traffic
         <<" near_duplicate="<<e.duplicate<<" event_seq="<<e.syncSequence<<" event_thread="<<e.thread;
        writeGuard("receiver",e.receiverChecks);writeGuard("other",e.otherChecks);
        s<<" sampled_context_match="<<e.contextMatches<<" age_ms="<<static_cast<double>(e.contextAgeUs)/1000.0
         <<" "<<e.context;
    }
    // Each trace includes strongest observed correction before/after the
    // event for each exact entity ID. Temporal proximity is NOT causation.
    auto outputMatch=[&](const char* label,const kineticle::TraceSample& m,std::int64_t t) {
        s<<" "<<label<<"=";
        if(!m.id){s<<"none";return;}
        s<<"{dt_ms:"<<static_cast<double>(m.us-t)/1000.0<<",seq:"<<m.sequence
         <<",thread:"<<m.thread<<",dv:"<<m.change<<",loss:"<<m.loss
         <<",before:("<<m.before.x<<","<<m.before.y<<","<<m.before.z<<")"
         <<",after:("<<m.after.x<<","<<m.after.y<<","<<m.after.z<<")"
         <<",angular_dv:"<<kineticle::length(kineticle::sub(m.angularAfter,m.angularBefore))
         <<",angular_before:("<<m.angularBefore.x<<","<<m.angularBefore.y<<","<<m.angularBefore.z<<")"
         <<",angular_after:("<<m.angularAfter.x<<","<<m.angularAfter.y<<","<<m.angularAfter.z<<")"
         <<",mass:"<<m.mass<<",inverse_mass:"<<m.inverseMass<<",mass_valid:"<<m.massOk
         <<",dt:"<<m.dt<<",physics_state:"<<m.physicsState<<",candidate:"<<m.candidate
         <<",force:("<<m.force.x<<","<<m.force.y<<","<<m.force.z<<")"
         <<",torque:("<<m.torque.x<<","<<m.torque.y<<","<<m.torque.z<<")";
        const auto estimate=kineticle::estimate(m.before,m.after,m.mass,m.massOk);
        s<<",estimated_impulse:"<<estimate.impulse<<",recover50:("<<estimate.recover50.x<<","<<estimate.recover50.y<<","<<estimate.recover50.z<<")"
         <<",recover100:("<<estimate.recover100.x<<","<<estimate.recover100.y<<","<<estimate.recover100.z<<")"
         <<",extra5x:"<<estimate.extra5<<",extra100x:"<<estimate.extra100<<"}";
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
    std::lock_guard lock(stateMutex);const auto ok=save(value);
    if(ok&&!value) {std::lock_guard samples(sampleMutex);experiment.store(false);if(trial.active)trial.cancel(kineticle::TrialReject::Disabled);publishTrialIds();}
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
void setExperiment(IScriptable*,CStackFrame* frame,bool* out,std::int64_t) {
    bool value=false;RED4ext::GetParameter(frame,&value);frame->code++;
    std::lock_guard lock(sampleMutex);
    if(value&&(!enabled.load()||!installed.load()||!eventInstalled.load())) {if(out)*out=false;return;}
    experiment.store(value);
    if(!value&&trial.active)trial.cancel(kineticle::TrialReject::Disabled);
    publishTrialIds();
    if(out)*out=true;
}
void setControllerContext(IScriptable*,CStackFrame* frame,bool* out,std::int64_t) {
    RED4ext::ent::EntityID id{};CString detail;bool allowed=false;
    RED4ext::GetParameter(frame,&id);RED4ext::GetParameter(frame,&detail);RED4ext::GetParameter(frame,&allowed);frame->code++;
    if(out)*out=false;
    const std::string value=detail.c_str()?detail.c_str():"";
    if(value.size()>512)return;
    std::lock_guard lock(sampleMutex);
    controllerContext={id.hash,nowUs(),value,allowed&&id.hash!=0};
    if(trial.active&&!contextAllows(trial.driver,controllerContext.us)) {
        trial.cancel(kineticle::TrialReject::Context);publishTrialIds();
    }
    if(out)*out=true;
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
        (experiment.load()?"; experimental motion enabled; see MOTION_TRIAL / MOTION_WRITE":"; experimental motion disabled"));
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
    calls.store(0);candidateFrames.store(0);corrections.store(0);last={};
    bumpCalls.store(0);rejectedStacks.store(0);invalidPairs.store(0);observedPairs.store(0);
    npcPairs.store(0);driverQueryFailures.store(0);playerPairs.store(0);flagDisagreements.store(0);
    guardedPairs.store(0);arrayStacks.store(0);packedStacks.store(0);lastPair={};lastDecode={};trace={};
    for(auto& counter:rejectCounts)counter.store(0);
    pairHistory={};pairIndex=0;controllerContext={};trial={};publishTrialIds();resetUs=nowUs();
    for(auto* counter:{&invalidOwner,&invalidVectors,&invalidMass,&invalidDt,&syncObserved,&traceSamples,
        &reentrantEvents,&duplicateEvents,&absentOther,&badContext,&badSignature,&badPair,
        &syncCostUs,&syncCostMaxUs,&eventCostUs,&eventCostMaxUs})counter->store(0);
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
    add("Kineticle_SetExperimentEnabled",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&setExperiment),"Bool","Bool","enabled");
    add("Kineticle_CaptureDiagnostics",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&captureReport),"Int32");
    add("Kineticle_DiagnosticPart",reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&getReportPart),"String","Int32","index");
    auto* contextFn=RED4ext::CGlobalFunction::Create("Kineticle_SetControllerContext","Kineticle_SetControllerContext",
        reinterpret_cast<RED4ext::ScriptingFunction_t<void*>>(&setControllerContext));
    if(contextFn) {
        contextFn->flags=RED4ext::CBaseFunction::Flags{.isNative=true,.isStatic=true};
        if(contextFn->SetReturnType("Bool")&&contextFn->AddParam("entEntityID","vehicle")&&
           contextFn->AddParam("String","detail")&&contextFn->AddParam("Bool","allowed"))rtti->RegisterFunction(contextFn);
    }
}
}
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle handle,RED4ext::EMainReason reason,const RED4ext::Sdk* sdk){
    if(reason==RED4ext::EMainReason::Unload){
        enabled.store(false);experiment.store(false);
        if(eventInstalled.load()&&sdk&&sdk->hooking&&sdk->hooking->Detach){
            sdk->hooking->Detach(handle,eventTarget);eventInstalled.store(false);
        }
        if(installed.load()&&sdk&&sdk->hooking&&sdk->hooking->Detach){
            sdk->hooking->Detach(handle,target);installed.store(false);
        }
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
