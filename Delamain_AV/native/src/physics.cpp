// Native physics bridge for the private Delamain AV pickup.
// Physics-tick signature and offsets informed by tidusmd's MIT-licensed
// CP2077-RED4EXT-DAV develop, commit 28a82418c9f956d2b3b06cdff575adc5a3752d63.
// See THIRD_PARTY_LICENSES.txt. No forked SDK, persistent vehicle handles or
// world transforms. Reverse-engineered pointer fields are cross-checked before use.
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/RTTITypes.hpp>
#include <RED4ext/Scripting/Natives/vehicleBaseObject.hpp>
#include <RED4ext/Scripting/Natives/Generated/vehicle/CarBaseObject.hpp>
#include <RED4ext/Scripting/Natives/Generated/Vector3.hpp>
#include <Windows.h>
#include <winver.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>
#include <algorithm>
#include "diagnostics.hpp"
#include "version.h"

namespace {
using namespace RED4ext;
using Vehicle = vehicle::BaseObject;
using TickFn = void (*)(void*, float);
using WakeFn = void (*)(Vehicle*);
using ForceFn = void (*)(void*, Vector3*, Vector3*);
constexpr uint32_t kForceHash = 611586815;
constexpr uint32_t kTorqueHash = 3303544265;
constexpr uint32_t kTickHash = 2526549425;
constexpr uint32_t kWakeHash = 2793083181;
// Actual DAV 3.5.0 DLL SHA256 a61ab1b4f42e79cccfff681d2699f9003793c95326526fb0c2d7c1eeeca0f3c4:
// RVA 0x9BF7 loads tick parent at +0x60, 0x9D13 loads parent physicsData at +0x2D8.
// dev.20 runtime logs independently prove tick parent matches our private entity.
// Do not read obsolete +0x2C8 or assume a reverse offset for WheeledPhysics.
constexpr size_t kPhysicsParent = 0x60;
constexpr size_t kVehicleBody = 0x2D8;
constexpr size_t kPhysicsState = 0x260;
constexpr size_t kOwner = 0x180;
constexpr size_t kVelocity = 0x18;
constexpr size_t kAngular = 0x24;
constexpr size_t kMass = 0x1BC;
constexpr size_t kGravity = 0x1B0;
constexpr size_t kOwnerId = 0x48;
const TweakDBID kRecord("Vehicle.m1nsook_delamain_av");
static_assert(offsetof(ent::Entity, entityID) == kOwnerId);
static_assert(sizeof(Vector3) == 12);

const v1::Sdk* g_sdk = nullptr;
v1::PluginHandle g_plugin{};
void* g_tickAddress = nullptr;
TickFn g_tickOriginal = nullptr;
void* g_forceAddress = nullptr;
void* g_torqueAddress = nullptr;
ForceFn g_forceOriginal = nullptr;
ForceFn g_torqueOriginal = nullptr;
WakeFn g_wake = nullptr;
std::mutex g_mutex;
std::atomic<bool> g_active{false};
std::atomic<bool> g_worldAttached{false};
std::atomic<bool> g_shuttingDown{false};
bool CanControl() { return !g_shuttingDown.load(std::memory_order_acquire) && g_worldAttached.load(std::memory_order_acquire); }
bool g_runtimeOK = false;
struct Command {
    uintptr_t owner = 0;
    uint64_t id = 0;
    uint64_t deadline = 0;
    uint64_t lastWake = 0;
    float desired = 0;
    float applied = 0;
    int32_t pendingBody = 0;
    int32_t pulses = 0;
    uint64_t lastTickLog = 0;
    int32_t samples = 0;
    int32_t fault = 0;
    uint64_t suppressedForce = 0;
    uint64_t suppressedTorque = 0;
};
Command g_cmd;

template<class T>
bool Read(const void* base, size_t offset, T& value) {
    SIZE_T n = 0;
    return base && ReadProcessMemory(GetCurrentProcess(),
        static_cast<const std::byte*>(base) + offset, &value, sizeof(T), &n) &&
        n == sizeof(T);
}
template<class T>
bool Write(void* base, size_t offset, const T& value) {
    SIZE_T n = 0;
    return base && WriteProcessMemory(GetCurrentProcess(),
        static_cast<std::byte*>(base) + offset, &value, sizeof(T), &n) &&
        n == sizeof(T);
}
bool Bounded(const Vector3& v) {
    return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z) &&
        std::abs(v.X) < 100 && std::abs(v.Y) < 100 && std::abs(v.Z) < 100;
}
float Approach(float value, float target, float step) {
    return value + std::clamp(target - value, -step, step);
}
void Fault(int32_t reason) {
    avdiag::Log("FAULT", "code=%d entity=%016llx owner=%p samples=%d pending=%d pulses=%d", reason, static_cast<unsigned long long>(g_cmd.id), reinterpret_cast<void*>(g_cmd.owner), g_cmd.samples, g_cmd.pendingBody, g_cmd.pulses);
    g_cmd.fault = reason;
    g_active.store(false, std::memory_order_release);
}
bool RuntimeOK() {
    wchar_t path[32768]{};
    auto n = GetModuleFileNameW(nullptr, path, 32768);
    if (!n || n >= 32768) return false;
    DWORD unused = 0;
    DWORD bytes = GetFileVersionInfoSizeW(path, &unused);
    if (!bytes) return false;
    std::vector<std::byte> buffer(bytes);
    if (!GetFileVersionInfoW(path, 0, bytes, buffer.data())) return false;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&info), &len) ||
        !info || len < sizeof(*info) || info->dwSignature != 0xFEEF04BD) return false;
    return HIWORD(info->dwFileVersionMS) == 3 && LOWORD(info->dwFileVersionMS) == 0 &&
        HIWORD(info->dwFileVersionLS) == 88 && LOWORD(info->dwFileVersionLS) == 46235;
}
bool Conflict() {
    return GetModuleHandleW(L"DriveAerialVehicle.dll") || GetModuleHandleW(L"FlyingTank.dll") ||
           GetModuleHandleW(L"LetThereBeFlight.dll");
}
void* Resolve(uint32_t hash) {
    // The SDK helper terminates the process on a missing hash. Fail closed instead.
    auto module = GetModuleHandleW(L"RED4ext.dll");
    if (!module) { avdiag::Log("RELOC", "hash=%u RED4ext.dll absent",hash); return nullptr; }
    using Resolver = uintptr_t (*)(uint32_t);
    auto resolver = reinterpret_cast<Resolver>(GetProcAddress(module, "RED4ext_ResolveAddress"));
    if (!resolver) { avdiag::Log("RELOC", "hash=%u address resolver absent",hash); return nullptr; }
    auto addr = reinterpret_cast<void*>(resolver(hash));
    MEMORY_BASIC_INFORMATION mbi{};
    if (!addr || !VirtualQuery(addr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        mbi.AllocationBase != GetModuleHandleW(nullptr) ||
        !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
        (mbi.Protect & PAGE_GUARD)) { avdiag::Log("RELOC", "hash=%u addr=%p rejected state=%lu protection=%lu",hash,addr,mbi.State,mbi.Protect); return nullptr; }
    avdiag::Log("RELOC", "hash=%u address=%p imageRVA=0x%llx",hash,addr,static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(addr)-reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))));
    return addr;
}
// Validate the reverse-engineered pointer chain on the live target.
// Never dereference a foreign vehicle's physicsData or cache a body pointer.
bool ResolveBody(uintptr_t owner, uintptr_t expectedPhysics, uintptr_t& body, bool verbose = false) {
    if (!owner) { if (verbose) avdiag::Log("BODY","owner absent"); return false; }
    auto* vehicle = reinterpret_cast<void*>(owner);
    uintptr_t parent = 0, reverseOwner = 0;
    body = 0;
    const char* failure = nullptr;
    // The engine callback itself supplies a real physics object; validate its
    // parent when available. Never infer the physics object from the old SDK.
    if (expectedPhysics &&
        !Read(reinterpret_cast<void*>(expectedPhysics),kPhysicsParent,parent))
        failure = "physics.parent read failed";
    else if (expectedPhysics && parent != owner) failure = "physics.parent mismatch";
    else if (!Read(vehicle,kVehicleBody,body)) failure = "vehicle.physicsData read failed";
    else if (!body || (body & (alignof(void*)-1))) failure = "vehicle.physicsData null or unaligned";
    else if (!Read(reinterpret_cast<void*>(body),kOwner,reverseOwner)) failure = "body.vehicle read failed";
    else if (reverseOwner != owner) failure = "body.vehicle mismatch";
    if (verbose) avdiag::Log("BODY","owner=%p tick=%p parent@+0x%zx=%p body@+0x%zx=%p reverseOwner@+0x%zx=%p verdict=%s",
        vehicle,reinterpret_cast<void*>(expectedPhysics),kPhysicsParent,reinterpret_cast<void*>(parent),
        kVehicleBody,reinterpret_cast<void*>(body),kOwner,reinterpret_cast<void*>(reverseOwner),failure?failure:"VALID");
    return failure == nullptr;
}
void Snapshot(const Handle<Vehicle>& v, const char* stage) {
    if (!v) { avdiag::Log("AV","stage=%s handle=null",stage); return; }
    auto* cls = v->GetNativeType();
    avdiag::Log("AV","stage=%s nativeType=%s owner=%p entity=%016llx",stage,
        cls?cls->GetName().ToString():"unknown",v.GetPtr(),static_cast<unsigned long long>(v->entityID.hash));
    uint32_t state=0;
    bool stateOK=Read(v.GetPtr(),kPhysicsState,state);
    avdiag::Log("AV","physicsState@+0x%zx readable=%d raw=0x%x",kPhysicsState,stateOK,state);
    uintptr_t body=0;
    if (ResolveBody(reinterpret_cast<uintptr_t>(v.GetPtr()),0,body,true)) {
        auto* p=reinterpret_cast<void*>(body);
        float mass=0; Vector3 velocity{}, angular{}; uint8_t gravity=0;
        bool m=Read(p,kMass,mass), l=Read(p,kVelocity,velocity), a=Read(p,kAngular,angular), g=Read(p,kGravity,gravity);
        avdiag::Log("AV","body=%p mass readable=%d value=%.6f velocity readable=%d xyz=%.6f,%.6f,%.6f angular readable=%d xyz=%.6f,%.6f,%.6f gravity readable=%d value=%u",
            p,m,mass,l,velocity.X,velocity.Y,velocity.Z,a,angular.X,angular.Y,angular.Z,g,static_cast<unsigned>(gravity));
    }
}
void ApplyCommand(void* physics, float deltaTime) {
    if (!CanControl() || !g_active.load(std::memory_order_acquire)) return;
    uintptr_t owner = 0;
    if (!Read(physics, kPhysicsParent, owner)) return;
    std::unique_lock guard(g_mutex, std::try_to_lock);
    if (!guard.owns_lock() || !CanControl() || !g_active.load(std::memory_order_relaxed) || owner != g_cmd.owner || !owner) return;
    if (GetTickCount64() > g_cmd.deadline) { Fault(-7); return; }
    uint64_t entityId = 0;
    if (!Read(reinterpret_cast<void*>(owner), kOwnerId, entityId) || entityId != g_cmd.id) {
        Fault(-8); return;
    }
    uintptr_t bodyAddress = 0;
    if (!ResolveBody(owner, reinterpret_cast<uintptr_t>(physics), bodyAddress, g_cmd.samples == 0 && g_cmd.pulses <= 2)) {
        // A materialized entity can still be initializing physics. Let the
        // bounded script watchdog wait for a valid reciprocal body.
        return;
    }
    auto* body = reinterpret_cast<void*>(bodyAddress);
    float mass = 0;
    Vector3 velocity{}, angular{};
    if (!Read(body, kMass, mass) || !std::isfinite(mass) || mass < 100 || mass > 100000 ||
        !Read(body, kVelocity, velocity) || !Read(body, kAngular, angular) ||
        !Bounded(velocity) || !Bounded(angular)) {
        Fault(-9); return;
    }
    if (!std::isfinite(deltaTime) || deltaTime <= 0.f || deltaTime > 0.25f) return;
    const float dt = std::min(deltaTime, 0.05f);
    // Like DAV develop mode 2, hold a velocity target on the actual physics
    // tick, before the original resistance calculation. Ramp the target
    // itself for a smooth start/stop rather than writing positions.
    g_cmd.applied = Approach(g_cmd.applied, g_cmd.desired, 1.5f * dt);
    // DAV mode 2 holds the entire velocity vector every physics tick.
    // dev.20 drift exceeded the corridor in just two script ticks; don't
    // gradually brake sideways motion. Smooth only the vertical target.
    const Vector3 incoming = velocity;
    const Vector3 incomingAngular = angular;
    velocity.X = 0.f;
    velocity.Y = 0.f;
    velocity.Z = g_cmd.applied;
    angular = Vector3{0.f,0.f,0.f};
    uint8_t gravity = 0;
    if (!Read(body, kGravity, gravity) || gravity > 1) {
        Fault(-12); return;
    }
    // DAV develop's velocity-hold descent disables gravity on the physics
    // tick; retain that state only on the private nonpersistent AV.
    const uint8_t disabled = 0;
    if (!CanControl() || !g_active.load(std::memory_order_acquire)) return;
    if (!Write(body, kGravity, disabled) ||
        !Write(body, kVelocity, velocity) || !Write(body, kAngular, angular)) {
        Fault(-10); return;
    }
    if (g_cmd.samples < 1000000) ++g_cmd.samples;
    auto now=GetTickCount64();
    if (g_cmd.samples <= 10 || now-g_cmd.lastTickLog >= 100) {
        g_cmd.lastTickLog=now;
        avdiag::Log("PHYSICS_TICK","sample=%d dt=%.6f mass=%.3f desired=%.6f applied=%.6f velocity=%.6f,%.6f,%.6f angular=%.6f,%.6f,%.6f priorGravity=%u",g_cmd.samples,deltaTime,mass,g_cmd.desired,g_cmd.applied,velocity.X,velocity.Y,velocity.Z,angular.X,angular.Y,angular.Z,static_cast<unsigned>(gravity));
        avdiag::Log("FORCE_STATE","sample=%d incomingV=%.6f,%.6f,%.6f incomingAngular=%.6f,%.6f,%.6f suppressedForce=%llu suppressedTorque=%llu",g_cmd.samples,incoming.X,incoming.Y,incoming.Z,incomingAngular.X,incomingAngular.Y,incomingAngular.Z,static_cast<unsigned long long>(g_cmd.suppressedForce),static_cast<unsigned long long>(g_cmd.suppressedTorque));
    }
}
// DAV develop ApplyForceAtPosition/ApplyTorqueAtPosition: bypass the
// original when flight owns that vehicle. Do not suppress collisions or
// touch a foreign vehicle, an expired command or an inactive world.
// Never hold our mutex while calling original engine code.
bool SuppressOriginal(void* body, bool torque) {
    if (!CanControl() || !g_active.load(std::memory_order_acquire)) return false;
    uintptr_t owner = 0;
    if (!Read(body,kOwner,owner) || !owner) return false;
    std::unique_lock guard(g_mutex,std::try_to_lock);
    if (!guard.owns_lock() || !CanControl() ||
        !g_active.load(std::memory_order_relaxed) || owner != g_cmd.owner ||
        GetTickCount64() > g_cmd.deadline) return false;
    uint64_t id=0;
    uintptr_t expected=0;
    if (!Read(reinterpret_cast<void*>(owner),kOwnerId,id) || id != g_cmd.id ||
        !ResolveBody(owner,0,expected) || expected != reinterpret_cast<uintptr_t>(body))
        return false;
    auto& count = torque ? g_cmd.suppressedTorque : g_cmd.suppressedForce;
    ++count;
    if (count <= 3) avdiag::Log("FORCE_SUPPRESSED","kind=%s count=%llu entity=%016llx body=%p",torque?"torque":"force",static_cast<unsigned long long>(count),static_cast<unsigned long long>(id),body);
    return true;
}
void ForceHook(void* body,Vector3* offset,Vector3* force) {
    if (!SuppressOriginal(body,false)) g_forceOriginal(body,offset,force);
}
void TorqueHook(void* body,Vector3* offset,Vector3* torque) {
    if (!SuppressOriginal(body,true)) g_torqueOriginal(body,offset,torque);
}
void TickHook(void* physics, float deltaTime) {
    ApplyCommand(physics, deltaTime);
    g_tickOriginal(physics, deltaTime);
}
bool Install() {
    if (!CanControl()) return false;
    if (g_tickAddress && g_forceAddress && g_torqueAddress && g_wake) return true;
    if (!g_sdk || !g_sdk->hooking || !g_sdk->hooking->Attach) return false;
    auto tick=Resolve(kTickHash), wake=Resolve(kWakeHash);
    auto force=Resolve(kForceHash), torque=Resolve(kTorqueHash);
    if (!tick || !wake || !force || !torque ||
        tick==wake || tick==force || tick==torque || wake==force || wake==torque || force==torque) return false;
    // Partial installation stays dormant: Begin cannot activate control
    // until all three registered hooks are installed. RED4ext owns cleanup.
    if (!g_forceAddress) {
        if (!g_sdk->hooking->Attach(g_plugin,force,reinterpret_cast<void*>(&ForceHook),
            reinterpret_cast<void**>(&g_forceOriginal))) return false;
        g_forceAddress=force;
        avdiag::Log("HOOK","force installed address=%p",force);
    }
    if (!g_torqueAddress) {
        if (!g_sdk->hooking->Attach(g_plugin,torque,reinterpret_cast<void*>(&TorqueHook),
            reinterpret_cast<void**>(&g_torqueOriginal))) return false;
        g_torqueAddress=torque;
        avdiag::Log("HOOK","torque installed address=%p",torque);
    }
    if (!g_tickAddress) {
        if (!g_sdk->hooking->Attach(g_plugin,tick,reinterpret_cast<void*>(&TickHook),
            reinterpret_cast<void**>(&g_tickOriginal))) return false;
        g_tickAddress=tick;
    }
    g_wake=reinterpret_cast<WakeFn>(wake);
    avdiag::Log("HOOK","resistance tick installed address=%p trampoline=%p wake=%p",tick,reinterpret_cast<void*>(g_tickOriginal),wake);
    return true;
}
bool PrivateAV(const Handle<Vehicle>& v) {
    if (!v) return false;
    auto* cls = v->GetNativeType();
    auto* av = CRTTISystem::Get()->GetClass("vehicleCarBaseObject");
    if (!cls || !av || !cls->IsA(av)) return false;
    auto* getRecord = cls->GetFunction("GetRecordID");
    TweakDBID record;
    return getRecord && ExecuteFunction(v.GetPtr(), getRecord, &record) &&
        record.name.hash == kRecord.name.hash && record.name.length == kRecord.name.length;
}
void Begin(IScriptable*, CStackFrame* frame, int32_t* out, int64_t) {
    Handle<Vehicle> v;
    GetParameter(frame, &v); frame->code++;
    if (!CanControl()) { if (out) *out = -30; return; }
    avdiag::Log("BEGIN","runtimeOK=%d conflict=%d",g_runtimeOK,Conflict());
    Snapshot(v,"before native Begin");
    int32_t code = 0;
    if (!g_runtimeOK) code = -1;
    else if (Conflict()) code = -2;
    else if (!PrivateAV(v) || !v->entityID.hash) code = -3;
    else if (!Install()) code = -4;
    else {
        {
            std::lock_guard guard(g_mutex);
            if (!CanControl()) { if (out) *out = -30; return; }
            if (g_active.load(std::memory_order_relaxed)) { if (out) *out = -5; return; }
            g_cmd = {};
            g_cmd.owner = reinterpret_cast<uintptr_t>(v.GetPtr());
            g_cmd.id = v->entityID.hash;
            g_cmd.deadline = GetTickCount64() + 1500;
            g_cmd.lastWake = GetTickCount64();
            g_active.store(true, std::memory_order_release);
        }
        // Wake once on the game thread. Pulse reasserts simulation only if
        // the validated target is parked, just as DAV develop does.
        avdiag::Log("WAKE","before initial ForceEnablePhysics owner=%p",v.GetPtr());
        if (!CanControl()) { g_active.store(false,std::memory_order_release); if (out) *out = -30; return; }
        g_wake(v.GetPtr());
        avdiag::Log("WAKE","after initial ForceEnablePhysics");
        Snapshot(v,"after native wake");
        code = 1;
    }
    avdiag::Log("BEGIN","result=%d",code);
    if (out) *out = code;
}
void Pulse(IScriptable*, CStackFrame* frame, int32_t* out, int64_t) {
    Handle<Vehicle> v; float speed = 0;
    GetParameter(frame, &v); GetParameter(frame, &speed); frame->code++;
    if (!CanControl()) { if (out) *out = -30; return; }
    int32_t code = -6;
    bool wake = false;
    {
        std::lock_guard guard(g_mutex);
        if (CanControl() && v && v->entityID.hash && v->entityID.hash == g_cmd.id &&
            reinterpret_cast<uintptr_t>(v.GetPtr()) == g_cmd.owner) {
            const auto now = GetTickCount64();
            if (g_cmd.fault) code = g_cmd.fault;
            else if (g_active.load(std::memory_order_relaxed) && now <= g_cmd.deadline &&
                     std::isfinite(speed) && speed >= -2.f && speed <= 0.f) {
                uintptr_t body = 0;
                uint32_t state = 0;
                // Bind first, then wake, then wait for the reciprocal
                // pointer chain; DAV also waits for simulation to initialize.
                ++g_cmd.pulses;
                const bool ready = ResolveBody(g_cmd.owner, 0, body, true);
                g_cmd.desired = speed;
                g_cmd.deadline = now + 1500;
                if (ready) {
                    g_cmd.pendingBody = 0;
                    code = g_cmd.samples;
                } else {
                    ++g_cmd.pendingBody;
                    // -20 is a nonterminal initialization status.
                    code = -20;
                    if (g_cmd.pendingBody >= 80) { Fault(-11); code = -11; }
                }
                const bool knownState = Read(v.GetPtr(), kPhysicsState, state) && state <= 0x1FF;
                avdiag::Log("PULSE","pulse=%d ready=%d samples=%d pending=%d stateKnown=%d rawState=0x%x target=%.6f result=%d",g_cmd.pulses,ready,g_cmd.samples,g_cmd.pendingBody,knownState,state,speed,code);
                if ((!ready || (knownState && state != 0)) && now - g_cmd.lastWake >= 250) {
                    g_cmd.lastWake = now;
                    wake = true;
                }
            } else { Fault(-7); code = -7; }
        }
    }
    // A live script argument holds the vehicle during this game-thread call.
    // Do not retain the handle or invoke the wake routine under the mutex.
    if (wake && g_wake && CanControl()) {
        avdiag::Log("WAKE","before repeated ForceEnablePhysics owner=%p",v.GetPtr());
        g_wake(v.GetPtr());
        avdiag::Log("WAKE","after repeated ForceEnablePhysics");
    }
    if (code < 0 && code != -20) avdiag::Log("PULSE","terminal result=%d",code);
    if (out) *out = code;
}
void Stop(IScriptable*, CStackFrame* frame, bool* out, int64_t) {
    EntityID id;
    GetParameter(frame, &id); frame->code++;
    std::lock_guard guard(g_mutex);
    bool ok = id.hash && id.hash == g_cmd.id;
    avdiag::Log("STOP","requested=%016llx matched=%d samples=%d fault=%d suppressedForce=%llu suppressedTorque=%llu",static_cast<unsigned long long>(id.hash),ok,g_cmd.samples,g_cmd.fault,static_cast<unsigned long long>(g_cmd.suppressedForce),static_cast<unsigned long long>(g_cmd.suppressedTorque));
    if (ok) { g_active.store(false, std::memory_order_release); g_cmd = {}; }
    if (out) *out = ok;
}
// World teardown never waits on the physics mutex and never deletes entities.
// Normal Stop still synchronizes with physics before explicit entity removal.
void WorldState(IScriptable*, CStackFrame* frame, bool* out, int64_t) {
    bool attached = false;
    GetParameter(frame, &attached); frame->code++;
    if (!attached) {
        g_worldAttached.store(false,std::memory_order_release);
        g_active.store(false,std::memory_order_release);
        avdiag::Log("LIFECYCLE","world detach: control disabled; no physics mutex wait");
    } else if (!g_shuttingDown.load(std::memory_order_acquire)) {
        g_active.store(false,std::memory_order_release);
        g_worldAttached.store(true,std::memory_order_release);
        avdiag::Log("LIFECYCLE","world attach: control idle");
    }
    if (out) *out=true;
}
void DiagnosticLog(IScriptable*, CStackFrame* frame, bool* out, int64_t) {
    CString level,message;
    GetParameter(frame,&level); GetParameter(frame,&message); frame->code++;
    if (!g_shuttingDown.load(std::memory_order_acquire)) avdiag::Log("SCRIPT","%s %s",level.c_str(),message.c_str());
    if (out) *out=true;
}
void DiagnosticSnapshot(IScriptable*, CStackFrame* frame, bool* out, int64_t) {
    Handle<Vehicle> v; CString stage;
    GetParameter(frame,&v); GetParameter(frame,&stage); frame->code++;
    // Only snapshot the passed private vehicle. No search of other entities.
    // Before class validation, the script logs only safe record/class casts.
    bool ok=CanControl() && PrivateAV(v);
    if (ok) Snapshot(v,stage.c_str());
    else avdiag::Log("AV","snapshot refused stage=%s private car record not confirmed",stage.c_str());
    if (out) *out=ok;
}
void DiagnosticEnvironment(IScriptable*, CStackFrame* frame, bool* out, int64_t) {
    CString stage; GetParameter(frame,&stage); frame->code++;
    if (CanControl()) avdiag::Inventory(stage.c_str());
    if (out) *out=true;
}
void Register() { avdiag::Log("RTTI","register callback"); }
void PostRegister() {
    auto* rtti = CRTTISystem::Get();
    avdiag::Log("RTTI","post-register begin");
    auto add = [&](const char* name, ScriptingFunction_t<void*> callback, const char* result,
                   const char* first, bool speed) {
        auto* fn = CGlobalFunction::Create(name, name, callback);
        fn->flags = CBaseFunction::Flags{.isNative = true, .isStatic = true};
        fn->SetReturnType(result);
        fn->AddParam(first, "target");
        if (speed) fn->AddParam("Float", "verticalSpeed");
        rtti->RegisterFunction(fn);
    };
    add("DelamainAV_PhysicsBegin", reinterpret_cast<ScriptingFunction_t<void*>>(&Begin),
        "Int32", "handle:vehicleBaseObject", false);
    add("DelamainAV_PhysicsPulse", reinterpret_cast<ScriptingFunction_t<void*>>(&Pulse),
        "Int32", "handle:vehicleBaseObject", true);
    add("DelamainAV_PhysicsStop", reinterpret_cast<ScriptingFunction_t<void*>>(&Stop),
        "Bool", "entEntityID", false);
    auto diagnostic = [&](const char* name,ScriptingFunction_t<void*> callback,
                          const char* first,const char* firstName,const char* secondName) {
        auto* fn=CGlobalFunction::Create(name,name,callback);
        fn->flags=CBaseFunction::Flags{.isNative=true,.isStatic=true};
        fn->SetReturnType("Bool");
        fn->AddParam(first,firstName);
        if(secondName) fn->AddParam("String",secondName);
        rtti->RegisterFunction(fn);
    };
    diagnostic("DelamainAV_WorldState",reinterpret_cast<ScriptingFunction_t<void*>>(&WorldState),"Bool","attached",nullptr);
    diagnostic("DelamainAV_DiagnosticLog",reinterpret_cast<ScriptingFunction_t<void*>>(&DiagnosticLog),"String","level","message");
    diagnostic("DelamainAV_DiagnosticSnapshot",reinterpret_cast<ScriptingFunction_t<void*>>(&DiagnosticSnapshot),"handle:vehicleBaseObject","target","stage");
    diagnostic("DelamainAV_DiagnosticEnvironment",reinterpret_cast<ScriptingFunction_t<void*>>(&DiagnosticEnvironment),"String","stage",nullptr);
    avdiag::Log("RTTI","post-register complete");
}
} // namespace
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle handle,
    RED4ext::v1::EMainReason reason, const RED4ext::v1::Sdk* sdk) {
    if (reason == RED4ext::v1::EMainReason::Load) {
        g_sdk = sdk; g_plugin = handle;
        const bool logging=avdiag::Open();
        avdiag::Log("SESSION","DelamainAV " DELAMAINAV_VERSION_STRING " diagnostics format 1; UTC; SDK vehicle size=0x%zx; expected runtime=3.0.88.46235; DAV 3.5.0 binary-confirmed body offset 0x2D8",sizeof(Vehicle));
        g_runtimeOK = RuntimeOK();
        avdiag::Log("SESSION","runtime gate=%d",g_runtimeOK);
        if (sdk && sdk->logger && sdk->logger->Info) sdk->logger->Info(handle,logging?"DelamainAV persistent diagnostics enabled: DelamainAV-diagnostics.log":"DelamainAV could not open persistent diagnostics file");
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti) return false;
        rtti->AddRegisterCallback(Register);
        rtti->AddPostRegisterCallback(PostRegister);
        if (sdk && sdk->logger && sdk->logger->Info)
            sdk->logger->Info(handle, g_runtimeOK ?
                "DelamainAV native bridge ready; hooks inactive until requested." :
                "DelamainAV native physics disabled: unsupported executable file version.");
    } else if (reason == RED4ext::v1::EMainReason::Unload) {
        // RED4ext v1.31.1 App::Shutdown invokes PluginSystem::Shutdown
        // before HookingSystem::Shutdown. The latter owns and retains a
        // shared_ptr to each hooked plugin until its detours are removed.
        // Do not initiate a second detour transaction or wait on game threads.
        g_shuttingDown.store(true,std::memory_order_release);
        g_worldAttached.store(false,std::memory_order_release);
        g_active.store(false,std::memory_order_release);
        avdiag::Log("SESSION","plugin unload: control disabled; hook cleanup delegated to RED4ext");
        avdiag::Close();
    }
    return true;
}
RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info) {
    info->name = L"Delamain AV Physics";
    info->author = L"MISO";
    info->version = RED4EXT_V1_SEMVER(
        DELAMAINAV_VERSION_MAJOR,
        DELAMAINAV_VERSION_MINOR,
        DELAMAINAV_VERSION_PATCH);
    info->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    info->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}
RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() { return RED4EXT_API_VERSION_1; }
