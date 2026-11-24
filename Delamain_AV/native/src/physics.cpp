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

namespace {
using namespace RED4ext;
using Vehicle = vehicle::BaseObject;
using TickFn = void (*)(void*, float);
using WakeFn = void (*)(Vehicle*);
constexpr uint32_t kTickHash = 2526549425;
constexpr uint32_t kWakeHash = 2793083181;
// DAV develop uses WheeledPhysics::parent -> BaseObject::physicsData.
// Check both reciprocal relationships and the exact entity pointer and ID.
constexpr size_t kPhysicsParent = 0x60;
constexpr size_t kVehiclePhysics = 0x2C8;
constexpr size_t kVehicleBody = 0x2D0;
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
WakeFn g_wake = nullptr;
std::mutex g_mutex;
std::atomic<bool> g_active{false};
bool g_runtimeOK = false;
struct Command {
    uintptr_t owner = 0;
    uint64_t id = 0;
    uint64_t deadline = 0;
    uint64_t lastWake = 0;
    float desired = 0;
    float applied = 0;
    int32_t pendingBody = 0;
    int32_t samples = 0;
    int32_t fault = 0;
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
    if (!module) return nullptr;
    using Resolver = uintptr_t (*)(uint32_t);
    auto resolver = reinterpret_cast<Resolver>(GetProcAddress(module, "RED4ext_ResolveAddress"));
    if (!resolver) return nullptr;
    auto addr = reinterpret_cast<void*>(resolver(hash));
    MEMORY_BASIC_INFORMATION mbi{};
    if (!addr || !VirtualQuery(addr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        mbi.AllocationBase != GetModuleHandleW(nullptr) ||
        !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
        (mbi.Protect & PAGE_GUARD)) return nullptr;
    return addr;
}
// Validate the reverse-engineered pointer chain on the live target.
// Never dereference a foreign vehicle's physicsData or cache a body pointer.
bool ResolveBody(uintptr_t owner, uintptr_t expectedPhysics, uintptr_t& body) {
    if (!owner) return false;
    auto* vehicle = reinterpret_cast<void*>(owner);
    uintptr_t physics = 0, parent = 0, reverseOwner = 0;
    if (!Read(vehicle, kVehiclePhysics, physics) || !physics ||
        (expectedPhysics && physics != expectedPhysics) ||
        !Read(reinterpret_cast<void*>(physics), kPhysicsParent, parent) || parent != owner ||
        !Read(vehicle, kVehicleBody, body) || !body ||
        !Read(reinterpret_cast<void*>(body), kOwner, reverseOwner) || reverseOwner != owner)
        return false;
    return true;
}
void ApplyCommand(void* physics, float deltaTime) {
    if (!g_active.load(std::memory_order_acquire)) return;
    uintptr_t owner = 0;
    if (!Read(physics, kPhysicsParent, owner)) return;
    std::lock_guard guard(g_mutex);
    if (!g_active.load(std::memory_order_relaxed) || owner != g_cmd.owner || !owner) return;
    if (GetTickCount64() > g_cmd.deadline) { Fault(-7); return; }
    uint64_t entityId = 0;
    if (!Read(reinterpret_cast<void*>(owner), kOwnerId, entityId) || entityId != g_cmd.id) {
        Fault(-8); return;
    }
    uintptr_t bodyAddress = 0;
    if (!ResolveBody(owner, reinterpret_cast<uintptr_t>(physics), bodyAddress)) {
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
    velocity.X = Approach(velocity.X, 0.f, 4.f * dt);
    velocity.Y = Approach(velocity.Y, 0.f, 4.f * dt);
    velocity.Z = g_cmd.applied;
    angular.X = Approach(angular.X, 0.f, 4.f * dt);
    angular.Y = Approach(angular.Y, 0.f, 4.f * dt);
    angular.Z = Approach(angular.Z, 0.f, 4.f * dt);
    uint8_t gravity = 0;
    if (!Read(body, kGravity, gravity) || gravity > 1) {
        Fault(-12); return;
    }
    // DAV develop's velocity-hold descent disables gravity on the physics
    // tick; retain that state only on the private nonpersistent AV.
    const uint8_t disabled = 0;
    if (!Write(body, kGravity, disabled) ||
        !Write(body, kVelocity, velocity) || !Write(body, kAngular, angular)) {
        Fault(-10); return;
    }
    if (g_cmd.samples < 1000000) ++g_cmd.samples;
}
void TickHook(void* physics, float deltaTime) {
    ApplyCommand(physics, deltaTime);
    g_tickOriginal(physics, deltaTime);
}
bool Install() {
    if (g_tickAddress && g_wake) return true;
    if (!g_sdk || !g_sdk->hooking || !g_sdk->hooking->Attach || !g_sdk->hooking->Detach) return false;
    auto tick = Resolve(kTickHash), wake = Resolve(kWakeHash);
    if (!tick || !wake || tick == wake) return false;
    if (!g_sdk->hooking->Attach(g_plugin, tick, reinterpret_cast<void*>(&TickHook),
         reinterpret_cast<void**>(&g_tickOriginal))) return false;
    g_tickAddress = tick;
    g_wake = reinterpret_cast<WakeFn>(wake);
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
    int32_t code = 0;
    if (!g_runtimeOK) code = -1;
    else if (Conflict()) code = -2;
    else if (!PrivateAV(v) || !v->entityID.hash) code = -3;
    else if (!Install()) code = -4;
    else {
        {
            std::lock_guard guard(g_mutex);
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
        g_wake(v.GetPtr());
        code = 1;
    }
    if (out) *out = code;
}
void Pulse(IScriptable*, CStackFrame* frame, int32_t* out, int64_t) {
    Handle<Vehicle> v; float speed = 0;
    GetParameter(frame, &v); GetParameter(frame, &speed); frame->code++;
    int32_t code = -6;
    bool wake = false;
    {
        std::lock_guard guard(g_mutex);
        if (v && v->entityID.hash && v->entityID.hash == g_cmd.id &&
            reinterpret_cast<uintptr_t>(v.GetPtr()) == g_cmd.owner) {
            const auto now = GetTickCount64();
            if (g_cmd.fault) code = g_cmd.fault;
            else if (g_active.load(std::memory_order_relaxed) && now <= g_cmd.deadline &&
                     std::isfinite(speed) && speed >= -2.f && speed <= 0.f) {
                uintptr_t body = 0;
                uint32_t state = 0;
                // Bind first, then wake, then wait for the reciprocal
                // pointer chain; DAV also waits for simulation to initialize.
                const bool ready = ResolveBody(g_cmd.owner, 0, body);
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
                if ((!ready || (knownState && state != 0)) && now - g_cmd.lastWake >= 250) {
                    g_cmd.lastWake = now;
                    wake = true;
                }
            } else { Fault(-7); code = -7; }
        }
    }
    // A live script argument holds the vehicle during this game-thread call.
    // Do not retain the handle or invoke the wake routine under the mutex.
    if (wake && g_wake) g_wake(v.GetPtr());
    if (out) *out = code;
}
void Stop(IScriptable*, CStackFrame* frame, bool* out, int64_t) {
    EntityID id;
    GetParameter(frame, &id); frame->code++;
    std::lock_guard guard(g_mutex);
    bool ok = id.hash && id.hash == g_cmd.id;
    if (ok) { g_active.store(false, std::memory_order_release); g_cmd = {}; }
    if (out) *out = ok;
}
void Register() {}
void PostRegister() {
    auto* rtti = CRTTISystem::Get();
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
}
} // namespace
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::v1::PluginHandle handle,
    RED4ext::v1::EMainReason reason, const RED4ext::v1::Sdk* sdk) {
    if (reason == RED4ext::v1::EMainReason::Load) {
        g_sdk = sdk; g_plugin = handle;
        g_runtimeOK = RuntimeOK();
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!rtti) return false;
        rtti->AddRegisterCallback(Register);
        rtti->AddPostRegisterCallback(PostRegister);
        if (sdk && sdk->logger && sdk->logger->Info)
            sdk->logger->Info(handle, g_runtimeOK ?
                "DelamainAV native bridge ready; hooks inactive until requested." :
                "DelamainAV native physics disabled: unsupported executable file version.");
    } else if (reason == RED4ext::v1::EMainReason::Unload) {
        g_active.store(false, std::memory_order_release);
        if (sdk && sdk->hooking && sdk->hooking->Detach) {
            if (g_tickAddress) sdk->hooking->Detach(handle, g_tickAddress);
        }
    }
    return true;
}
RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info) {
    info->name = L"Delamain AV Physics";
    info->author = L"MISO";
    info->version = RED4EXT_V1_SEMVER(0, 2, 11);
    info->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    info->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}
RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() { return RED4EXT_API_VERSION_1; }
