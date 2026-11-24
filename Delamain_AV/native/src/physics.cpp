// Native physics bridge for the private Delamain AV pickup.
// Rigid-body offsets and relocation hashes informed by tidusmd's MIT-licensed
// CP2077-RED4EXT-DAV, commit 9f0d1359a7f5bb02d71ac6fec86c36aef9dd5615,
// and the supplied DAV 3.5.0 binary. See THIRD_PARTY_LICENSES.txt.
// No forked vehicleBaseObject layout, persistent vehicle handles or world transforms.
#include <RED4ext/RED4ext.hpp>
#include <RED4ext/RTTITypes.hpp>
#include <RED4ext/Scripting/Natives/vehicleBaseObject.hpp>
#include <RED4ext/Scripting/Natives/Generated/vehicle/AVBaseObject.hpp>
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
using ForceFn = void (*)(void*, Vector3*, Vector3*);
using WakeFn = void (*)(Vehicle*);
constexpr uint32_t kForceHash = 611586815;
constexpr uint32_t kTorqueHash = 3303544265;
constexpr uint32_t kWakeHash = 2793083181;
// PhysicsData, not BaseObject. Owner and hash are corroborated by DAV 3.5.0.
constexpr size_t kOwner = 0x180;
constexpr size_t kVelocity = 0x18;
constexpr size_t kAngular = 0x24;
constexpr size_t kMass = 0x1BC;
constexpr size_t kOwnerId = 0x48;
const TweakDBID kRecord("Vehicle.m1nsook_delamain_av");
static_assert(offsetof(ent::Entity, entityID) == kOwnerId);
static_assert(sizeof(Vector3) == 12);

const v1::Sdk* g_sdk = nullptr;
v1::PluginHandle g_plugin{};
void* g_forceAddress = nullptr;
void* g_torqueAddress = nullptr;
ForceFn g_forceOriginal = nullptr;
ForceFn g_torqueOriginal = nullptr;
WakeFn g_wake = nullptr;
std::mutex g_mutex;
std::atomic<bool> g_active{false};
bool g_runtimeOK = false;
struct Command {
    uintptr_t owner = 0;
    uint64_t id = 0;
    uint64_t deadline = 0;
    uint64_t last = 0;
    float desired = 0;
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
void ApplyCommand(void* body) {
    if (!g_active.load(std::memory_order_acquire)) return;
    uintptr_t owner = 0;
    if (!Read(body, kOwner, owner)) return;
    std::lock_guard guard(g_mutex);
    if (!g_active.load(std::memory_order_relaxed) || owner != g_cmd.owner || !owner) return;
    auto now = GetTickCount64();
    if (now > g_cmd.deadline) { Fault(-7); return; }
    uint64_t entityId = 0;
    if (!Read(reinterpret_cast<void*>(owner), kOwnerId, entityId) || entityId != g_cmd.id) {
        Fault(-8); return;
    }
    // Only validated engine-owned physics objects are accessed. All original
    // force/torque callbacks still execute, including collision handling.
    float mass = 0;
    Vector3 velocity{}, angular{};
    if (!Read(body, kMass, mass) || !std::isfinite(mass) || mass < 100 || mass > 100000 ||
        !Read(body, kVelocity, velocity) || !Read(body, kAngular, angular) ||
        !Bounded(velocity) || !Bounded(angular)) {
        Fault(-9); return;
    }
    // Don't multiply acceleration when several forces are applied in one tick.
    if (g_cmd.last && now - g_cmd.last < 8) return;
    float dt = g_cmd.last ? std::clamp(float(now - g_cmd.last) * .001f, .008f, .05f) : .016f;
    g_cmd.last = now;
    // Bounded, gradual velocity change, not a position update. Keep original
    // gravity and original engine force/torque functions enabled.
    velocity.X = Approach(velocity.X, 0.f, 4.f * dt);
    velocity.Y = Approach(velocity.Y, 0.f, 4.f * dt);
    velocity.Z = Approach(velocity.Z, g_cmd.desired, 16.f * dt);
    angular.X = Approach(angular.X, 0.f, 4.f * dt);
    angular.Y = Approach(angular.Y, 0.f, 4.f * dt);
    angular.Z = Approach(angular.Z, 0.f, 4.f * dt);
    if (!Write(body, kVelocity, velocity) || !Write(body, kAngular, angular)) {
        Fault(-10); return;
    }
    if (g_cmd.samples < 1000000) ++g_cmd.samples;
}
void ForceHook(void* body, Vector3* pos, Vector3* force) {
    g_forceOriginal(body, pos, force);
    ApplyCommand(body);
}
void TorqueHook(void* body, Vector3* pos, Vector3* torque) {
    g_torqueOriginal(body, pos, torque);
    ApplyCommand(body);
}
bool Install() {
    if (g_forceAddress && g_torqueAddress && g_wake) return true;
    if (!g_sdk || !g_sdk->hooking || !g_sdk->hooking->Attach || !g_sdk->hooking->Detach) return false;
    auto force = Resolve(kForceHash), torque = Resolve(kTorqueHash), wake = Resolve(kWakeHash);
    if (!force || !torque || !wake || force == torque) return false;
    if (!g_sdk->hooking->Attach(g_plugin, force, reinterpret_cast<void*>(&ForceHook),
         reinterpret_cast<void**>(&g_forceOriginal))) return false;
    g_forceAddress = force;
    if (!g_sdk->hooking->Attach(g_plugin, torque, reinterpret_cast<void*>(&TorqueHook),
         reinterpret_cast<void**>(&g_torqueOriginal))) {
        g_sdk->hooking->Detach(g_plugin, g_forceAddress);
        g_forceAddress = nullptr;
        return false;
    }
    g_torqueAddress = torque;
    g_wake = reinterpret_cast<WakeFn>(wake);
    return true;
}
bool PrivateAV(const Handle<Vehicle>& v) {
    if (!v) return false;
    auto* cls = v->GetNativeType();
    auto* av = CRTTISystem::Get()->GetClass("vehicleAVBaseObject");
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
            g_active.store(true, std::memory_order_release);
        }
        // Called once from the REDscript/game thread, never a retry loop.
        // The current DAV binary and source both use this relocation hash.
        g_wake(v.GetPtr());
        code = 1;
    }
    if (out) *out = code;
}
void Pulse(IScriptable*, CStackFrame* frame, int32_t* out, int64_t) {
    EntityID id; float speed = 0;
    GetParameter(frame, &id); GetParameter(frame, &speed); frame->code++;
    std::lock_guard guard(g_mutex);
    int32_t code = -6;
    if (id.hash && id.hash == g_cmd.id) {
        if (g_cmd.fault) code = g_cmd.fault;
        else if (g_active.load(std::memory_order_relaxed) && GetTickCount64() <= g_cmd.deadline &&
                 std::isfinite(speed) && speed >= -2.f && speed <= 0.f) {
            g_cmd.desired = speed;
            g_cmd.deadline = GetTickCount64() + 1500;
            code = g_cmd.samples;
        } else { Fault(-7); code = -7; }
    }
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
        "Int32", "entEntityID", true);
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
            if (g_torqueAddress) sdk->hooking->Detach(handle, g_torqueAddress);
            if (g_forceAddress) sdk->hooking->Detach(handle, g_forceAddress);
        }
    }
    return true;
}
RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::v1::PluginInfo* info) {
    info->name = L"Delamain AV Physics";
    info->author = L"MISO";
    info->version = RED4EXT_V1_SEMVER(0, 2, 9);
    info->runtime = RED4EXT_V1_RUNTIME_VERSION_LATEST;
    info->sdk = RED4EXT_V1_SDK_VERSION_CURRENT;
}
RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports() { return RED4EXT_API_VERSION_1; }
