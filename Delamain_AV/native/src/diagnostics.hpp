#pragma once
// Persistent crash-surviving diagnostics. No uploads, no process memory dumps,
// no user paths, environment variables, save contents or account identifiers.
//
// Profiles (env var DELAMAINAV_DIAG before launch):
//   unset / "default"  session, RTTI, lifecycle, script, env, warnings,
//                      interesting modules, own files, and all physics events
//   "verbose"          everything above plus every loaded module and every
//                      .archive/.xl/.dll/.reds/.yaml on disk, plus the full
//                      per-tick physics trace (bounded at 32 MB)
//   "quiet"            session, lifecycle, script, warnings, faults only

#include <Windows.h>
#include <TlHelp32.h>
#include <winver.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <mutex>
#include <atomic>
#include <string>
#include <vector>
#include <system_error>

namespace avdiag {

enum Cat : uint32_t {
    C_Session   = 1u << 0,
    C_Rtti      = 1u << 1,
    C_Lifecycle = 1u << 2,
    C_Script    = 1u << 3,
    C_Env       = 1u << 4,
    C_Warn      = 1u << 5,
    C_Physics   = 1u << 6,   // BEGIN/STOP/PULSE/WAKE/HOOK/RELOC/AV/BODY
    C_Fault     = 1u << 7,   // FAULT (always on in every profile)
    C_Module    = 1u << 8,   // every loaded module (verbose only)
    C_Installed = 1u << 9,   // every file on disk (verbose only)
    C_Tick      = 1u << 10,  // PHYSICS_TICK / FORCE_STATE / FORCE_SUPPRESSED

    C_Default = C_Session | C_Rtti | C_Lifecycle | C_Script | C_Env
              | C_Warn    | C_Physics | C_Fault | C_Tick,
    C_Quiet   = C_Session | C_Lifecycle | C_Script | C_Warn | C_Fault,
    C_All     = 0xFFFFFFFFu,
};

inline std::atomic<uint32_t> enabled{ C_Default };
inline std::atomic<bool>     closing{ false };
inline std::atomic<uint64_t> dropped{ 0 };
inline HANDLE                file = INVALID_HANDLE_VALUE;
inline std::filesystem::path folder;
inline std::filesystem::path gameRoot;
inline uint64_t              sequence = 0;
inline uint64_t              bytes    = 0;
inline std::mutex            mutex;

inline std::atomic<bool> modulesDumped{ false };
inline std::atomic<bool> installedDumped{ false };

inline uint64_t MaxBytes() noexcept {
    return (enabled.load(std::memory_order_relaxed) & (C_Module | C_Installed))
        ? 32ull * 1024 * 1024
        :  2ull * 1024 * 1024;
}

// Maps the category string that every existing call site already passes.
// Unknown categories return 0 and are always emitted — keeps this file
// forward-compatible with new call sites that haven't been categorized.
inline uint32_t CategoryBit(const char* c) noexcept {
    if (!c) return 0;
    if (std::strcmp(c, "SESSION")           == 0) return C_Session;
    if (std::strcmp(c, "RTTI")              == 0) return C_Rtti;
    if (std::strcmp(c, "LIFECYCLE")         == 0) return C_Lifecycle;
    if (std::strcmp(c, "SCRIPT")            == 0) return C_Script;
    if (std::strcmp(c, "ENV")               == 0) return C_Env;
    if (std::strcmp(c, "WARN")              == 0) return C_Warn;
    if (std::strcmp(c, "MODULE")            == 0) return C_Module;
    if (std::strcmp(c, "INSTALLED")         == 0) return C_Installed;
    if (std::strcmp(c, "PHYSICS_TICK")      == 0) return C_Tick;
    if (std::strcmp(c, "BODY_POSE")         == 0) return C_Tick;
    if (std::strcmp(c, "FORCE_STATE")       == 0) return C_Tick;
    if (std::strcmp(c, "FORCE_SUPPRESSED")  == 0) return C_Tick;
    if (std::strcmp(c, "BEGIN")             == 0) return C_Physics;
    if (std::strcmp(c, "STOP")              == 0) return C_Physics;
    if (std::strcmp(c, "PULSE")             == 0) return C_Physics;
    if (std::strcmp(c, "WAKE")              == 0) return C_Physics;
    if (std::strcmp(c, "HOOK")              == 0) return C_Physics;
    if (std::strcmp(c, "RELOC")             == 0) return C_Physics;
    if (std::strcmp(c, "AV")                == 0) return C_Physics;
    if (std::strcmp(c, "BODY")              == 0) return C_Physics;
    if (std::strcmp(c, "FAULT")             == 0) return C_Fault;
    return 0;
}

inline std::string Utf8(const wchar_t* text) {
    if (!text || !*text) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string result(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), n, nullptr, nullptr);
    result.pop_back();
    return result;
}

inline std::string Version(const wchar_t* path) {
    DWORD unused = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &unused);
    if (!size || size > 1024 * 1024) return "unavailable";
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path, 0, size, data.data())) return "unavailable";
    VS_FIXEDFILEINFO* v = nullptr;
    UINT len = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&v), &len) ||
        !v || len < sizeof(*v) || v->dwSignature != 0xFEEF04BD) return "unavailable";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", HIWORD(v->dwFileVersionMS),
        LOWORD(v->dwFileVersionMS), HIWORD(v->dwFileVersionLS), LOWORD(v->dwFileVersionLS));
    return buf;
}

inline void Log(const char* category, const char* format, ...) noexcept {
    uint32_t bit = CategoryBit(category);
    if (bit && !(enabled.load(std::memory_order_relaxed) & bit)) return;
    try {
        char message[8192]{};
        va_list args; va_start(args, format);
        std::vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        for (char* p = message; *p; ++p) if (*p == '\r' || *p == '\n') *p = ' ';

        if (closing.load(std::memory_order_acquire)) return;
        std::unique_lock guard(mutex, std::try_to_lock);
        if (!guard.owns_lock()) { dropped.fetch_add(1, std::memory_order_relaxed); return; }
        if (closing.load(std::memory_order_acquire) || file == INVALID_HANDLE_VALUE) return;

        // Avoid the atomic RMW unless a drop actually happened.
        uint64_t skipped = 0;
        if (dropped.load(std::memory_order_relaxed) != 0)
            skipped = dropped.exchange(0, std::memory_order_relaxed);

        SYSTEMTIME time{}; GetSystemTime(&time);
        char line[8704]{};
        int n = std::snprintf(line, sizeof(line),
            "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ seq=%llu mono=%llu tid=%lu skipped=%llu [%s] %s\r\n",
            time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,
            static_cast<unsigned long long>(++sequence),
            static_cast<unsigned long long>(GetTickCount64()),
            static_cast<unsigned long>(GetCurrentThreadId()),
            static_cast<unsigned long long>(skipped),
            category, message);
        if (n <= 0) return;
        DWORD count = static_cast<DWORD>((std::min)(static_cast<size_t>(n), sizeof(line) - 1));

        if (bytes + count > MaxBytes()) {
            CloseHandle(file); file = INVALID_HANDLE_VALUE;
            MoveFileExW((folder / L"DelamainAV-diagnostics.log").c_str(),
                        (folder / L"DelamainAV-diagnostics-overflow.log").c_str(),
                        MOVEFILE_REPLACE_EXISTING);
            file = CreateFileW((folder / L"DelamainAV-diagnostics.log").c_str(),
                               GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            bytes = 0;
            if (file == INVALID_HANDLE_VALUE) return;
        }
        DWORD written = 0;
        if (WriteFile(file, line, count, &written, nullptr)) bytes += written;
    } catch (...) { /* Diagnostics must never interrupt gameplay. */ }
}

inline void Configure() noexcept {
    wchar_t buf[16]{};
    DWORD n = GetEnvironmentVariableW(L"DELAMAINAV_DIAG", buf, 16);
    const char* profile = "default";
    if (n > 0 && n < 16) {
        if (_wcsicmp(buf, L"verbose") == 0 || _wcsicmp(buf, L"full") == 0) {
            enabled.store(C_All, std::memory_order_relaxed);
            profile = "verbose";
        } else if (_wcsicmp(buf, L"quiet") == 0) {
            enabled.store(C_Quiet, std::memory_order_relaxed);
            profile = "quiet";
        }
    }
    Log("SESSION", "diagnostics profile=%s cap=%lluKiB", profile,
        static_cast<unsigned long long>(MaxBytes() / 1024));
}

inline bool Open() noexcept {
    try {
        wchar_t path[32768]{};
        auto mod = GetModuleHandleW(L"DelamainAV.dll");
        if (!mod) return false;
        DWORD n = GetModuleFileNameW(mod, path, 32768);
        if (!n || n >= 32768) return false;
        folder = std::filesystem::path(path).parent_path();

        // Expected layout: <game>\bin\x64\Cyberpunk2077.exe. Verify instead of
        // silently walking three parents up — a mislaid install otherwise
        // produces an empty gameRoot and the inventory logs nothing useful.
        n = GetModuleFileNameW(nullptr, path, 32768);
        if (n && n < 32768) {
            auto exeDir = std::filesystem::path(path).parent_path();
            if (exeDir.filename() == L"x64" && exeDir.parent_path().filename() == L"bin")
                gameRoot = exeDir.parent_path().parent_path();
        }

        DeleteFileW((folder / L"DelamainAV-diagnostics-overflow.log").c_str());

        auto current  = folder / L"DelamainAV-diagnostics.log";
        auto previous = folder / L"DelamainAV-diagnostics-previous.log";
        MoveFileExW(current.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING);

        file = CreateFileW(current.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);

        Configure();
        return file != INVALID_HANDLE_VALUE;
    } catch (...) { return false; }
}

inline void Close() noexcept {
    closing.store(true, std::memory_order_release);
    std::unique_lock guard(mutex, std::try_to_lock);
    if (guard.owns_lock() && file != INVALID_HANDLE_VALUE) {
        CloseHandle(file); file = INVALID_HANDLE_VALUE;
    }
}

// ---------------------------------------------------------------- inventory

inline void SampleMemory() noexcept {
    MEMORYSTATUSEX mem{}; mem.dwLength = sizeof(mem);
    if (!GlobalMemoryStatusEx(&mem)) return;
    Log("ENV", "physicalMemoryMiB=%llu availableMemoryMiB=%llu memoryLoad=%lu%%",
        static_cast<unsigned long long>(mem.ullTotalPhys / 1048576),
        static_cast<unsigned long long>(mem.ullAvailPhys / 1048576),
        static_cast<unsigned long>(mem.dwMemoryLoad));
    if (mem.ullAvailPhys < 512ull * 1024 * 1024 || mem.dwMemoryLoad >= 90) {
        Log("WARN", "memory pressure availableMiB=%llu load=%lu%%",
            static_cast<unsigned long long>(mem.ullAvailPhys / 1048576),
            static_cast<unsigned long>(mem.dwMemoryLoad));
    }
}

inline void DumpStaticEnvironment() noexcept {
    SYSTEM_INFO si{}; GetNativeSystemInfo(&si);
    Log("ENV", "cpuLogical=%lu architecture=%u",
        static_cast<unsigned long>(si.dwNumberOfProcessors), si.wProcessorArchitecture);

    using RtlVersion = LONG (WINAPI*)(OSVERSIONINFOW*);
    auto rtl = reinterpret_cast<RtlVersion>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
    OSVERSIONINFOW os{}; os.dwOSVersionInfoSize = sizeof(os);
    if (rtl && rtl(&os) == 0)
        Log("ENV", "Windows=%lu.%lu build=%lu",
            static_cast<unsigned long>(os.dwMajorVersion),
            static_cast<unsigned long>(os.dwMinorVersion),
            static_cast<unsigned long>(os.dwBuildNumber));
}

inline bool InterestingModule(const MODULEENTRY32W& e) noexcept {
    if (_wcsicmp(e.szModule, L"Cyberpunk2077.exe") == 0) return true;
    if (_wcsicmp(e.szModule, L"DelamainAV.dll")    == 0) return true;

    static const wchar_t* kKeep[] = {
        L"red4ext.dll",  L"ArchiveXL.dll", L"TweakXL.dll",
        L"Codeware.dll", L"input_loader.dll", L"m1arc.dll",
        L"audioware.dll", L"cyber_engine_tweaks.asi",
        L"AdvancedCrashReporter.asi",
    };
    for (auto* n : kKeep) if (_wcsicmp(e.szModule, n) == 0) return true;

    if (!gameRoot.empty()) {
        size_t len = gameRoot.native().size();
        if (_wcsnicmp(e.szExePath, gameRoot.c_str(), len) == 0) return true;
    }
    return false;
}

inline bool OwnedByMod(const std::wstring& rel) noexcept {
    static const wchar_t* kNeedles[] = {
        L"\\DelamainAV\\", L"\\DelamainAV.",
        L"\\DriveAerialVehicle\\", L"\\DriveAerialVehicle.",
        L"\\m1arc\\", L"\\m1arc.",
        L"\\M1ArcCarryWeight", L"\\M1ArcSkipBreach", L"\\M1Arc.",
    };
    for (auto* n : kNeedles) if (rel.find(n) != std::wstring::npos) return true;
    return false;
}

inline void DumpModulesOnce() noexcept {
    if (modulesDumped.exchange(true)) return;
    HANDLE snap = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) {
        Log("ENV", "module snapshot failed error=%lu", GetLastError());
        return;
    }
    const bool showAll = (enabled.load(std::memory_order_relaxed) & C_Module) != 0;
    size_t total = 0, kept = 0;
    MODULEENTRY32W e{}; e.dwSize = sizeof(e);
    if (Module32FirstW(snap, &e)) do {
        ++total;
        if (!showAll && !InterestingModule(e)) continue;
        auto name    = Utf8(e.szModule);
        auto version = Version(e.szExePath);
        Log("MODULE", "name=%s version=%s image=%p size=%lu",
            name.c_str(), version.c_str(),
            static_cast<void*>(e.modBaseAddr),
            static_cast<unsigned long>(e.modBaseSize));
        ++kept;
    } while (Module32NextW(snap, &e));
    CloseHandle(snap);
    Log("ENV", "modules total=%zu logged=%zu", total, kept);
}

inline void DumpInstalledOnce() noexcept {
    if (installedDumped.exchange(true)) return;
    if (gameRoot.empty()) return;
    const bool showAll = (enabled.load(std::memory_order_relaxed) & C_Installed) != 0;

    for (auto relative : { L"archive\\pc\\mod", L"red4ext\\plugins",
                           L"r6\\scripts",      L"r6\\tweaks", L"mods" }) {
        auto dir = gameRoot / relative;
        std::error_code ec;
        size_t total = 0, listed = 0;
        uint64_t totalBytes = 0;

        std::filesystem::recursive_directory_iterator it(
            dir, std::filesystem::directory_options::skip_permission_denied, ec), end;
        while (!ec && it != end && total < 20000) {
            if (it.depth() > 6 || it->is_symlink(ec)) it.disable_recursion_pending();
            if (ec) break;
            if (it->is_regular_file(ec)) {
                auto ext = it->path().extension().wstring();
                if (ext == L".archive" || ext == L".xl"   || ext == L".dll"  ||
                    ext == L".reds"    || ext == L".yaml" || ext == L".yml"  ||
                    ext == L".tweak"   || ext == L".xlz") {
                    ++total;
                    auto size = it->file_size(ec);
                    if (!ec) totalBytes += size;

                    auto rel = it->path().lexically_relative(gameRoot).wstring();
                    if (showAll || OwnedByMod(rel)) {
                        Log("INSTALLED", "%s bytes=%llu",
                            Utf8(rel.c_str()).c_str(),
                            static_cast<unsigned long long>(ec ? 0 : size));
                        ++listed;
                    }
                }
            }
            if (ec) break;
            it.increment(ec);
        }
        Log("ENV", "installed folder=%s total=%zu listed=%zu bytes=%llu error=%d",
            Utf8(relative).c_str(), total, listed,
            static_cast<unsigned long long>(totalBytes), ec.value());
    }
}

inline void Inventory(const char* stage) noexcept {
    try {
        Log("ENV", "begin stage=%s", stage);
        SampleMemory();              // every call — memory changes
        DumpStaticEnvironment();     // cheap: two syscalls
        DumpModulesOnce();           // one-shot
        DumpInstalledOnce();         // one-shot
        Log("ENV", "end stage=%s", stage);
    } catch (...) {
        Log("ENV", "inventory interrupted stage=%s", stage);
    }
}

} // namespace avdiag