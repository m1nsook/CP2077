#include <algorithm>
#pragma once
// Persistent crash-surviving diagnostics. No uploads, no process memory dumps,
// no user paths, environment variables, save contents or account identifiers.
#include <Windows.h>
#include <TlHelp32.h>
#include <winver.h>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include <system_error>

namespace avdiag {
inline std::mutex mutex;
inline HANDLE file = INVALID_HANDLE_VALUE;
inline std::filesystem::path folder;
inline std::filesystem::path gameRoot;
inline uint64_t sequence = 0;
inline uint64_t bytes = 0;
inline constexpr uint64_t maxBytes = 32ull * 1024 * 1024;

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
    try {
        char message[8192]{};
        va_list args; va_start(args, format);
        std::vsnprintf(message, sizeof(message), format, args);
        va_end(args);
        for (char* p = message; *p; ++p) if (*p == '\r' || *p == '\n') *p = ' ';
        std::lock_guard guard(mutex);
        if (file == INVALID_HANDLE_VALUE) return;
        SYSTEMTIME time{}; GetSystemTime(&time);
        char line[8704]{};
        int n = std::snprintf(line, sizeof(line),
            "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ seq=%llu mono=%llu tid=%lu [%s] %s\r\n",
            time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,
            static_cast<unsigned long long>(++sequence),static_cast<unsigned long long>(GetTickCount64()),
            static_cast<unsigned long>(GetCurrentThreadId()),category,message);
        if (n <= 0) return;
        DWORD count = static_cast<DWORD>((std::min)(static_cast<size_t>(n),sizeof(line)-1));
        if (bytes + count > maxBytes) {
            CloseHandle(file); file = INVALID_HANDLE_VALUE;
            MoveFileExW((folder / L"DelamainAV-diagnostics.log").c_str(),
                (folder / L"DelamainAV-diagnostics-overflow.log").c_str(), MOVEFILE_REPLACE_EXISTING);
            file = CreateFileW((folder / L"DelamainAV-diagnostics.log").c_str(), GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            bytes = 0;
            if (file == INVALID_HANDLE_VALUE) return;
        }
        // Unbuffered at the application level: records reach the OS immediately,
        // including the last pre-call breadcrumb if the game process crashes.
        DWORD written = 0;
        if (WriteFile(file,line,count,&written,nullptr)) bytes += written;
    } catch (...) { /* Diagnostics must never interrupt gameplay. */ }
}
inline bool Open() noexcept {
    try {
        wchar_t path[32768]{};
        auto mod = GetModuleHandleW(L"DelamainAV.dll");
        if (!mod) return false;
        DWORD n = GetModuleFileNameW(mod,path,32768);
        if (!n || n >= 32768) return false;
        folder = std::filesystem::path(path).parent_path();
        n = GetModuleFileNameW(nullptr,path,32768);
        if (n && n < 32768)
            gameRoot = std::filesystem::path(path).parent_path().parent_path().parent_path();
        auto current = folder / L"DelamainAV-diagnostics.log";
        auto previous = folder / L"DelamainAV-diagnostics-previous.log";
        // Preserve previous crash session until the next game launch.
        MoveFileExW(current.c_str(),previous.c_str(),MOVEFILE_REPLACE_EXISTING);
        file = CreateFileW(current.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        return file != INVALID_HANDLE_VALUE;
    } catch (...) { return false; }
}
inline void Close() noexcept {
    std::lock_guard guard(mutex);
    if (file != INVALID_HANDLE_VALUE) { CloseHandle(file); file = INVALID_HANDLE_VALUE; }
}
inline void Inventory(const char* stage) noexcept {
    try {
        Log("ENV","begin stage=%s",stage);
        SYSTEM_INFO si{}; GetNativeSystemInfo(&si);
        MEMORYSTATUSEX mem{}; mem.dwLength=sizeof(mem);
        if (GlobalMemoryStatusEx(&mem))
            Log("ENV","cpuLogical=%lu architecture=%u physicalMemoryMiB=%llu availableMemoryMiB=%llu memoryLoad=%lu%%",
                static_cast<unsigned long>(si.dwNumberOfProcessors),si.wProcessorArchitecture,
                static_cast<unsigned long long>(mem.ullTotalPhys/1048576),
                static_cast<unsigned long long>(mem.ullAvailPhys/1048576),
                static_cast<unsigned long>(mem.dwMemoryLoad));
        using RtlVersion = LONG (WINAPI*)(OSVERSIONINFOW*);
        auto rtl = reinterpret_cast<RtlVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion"));
        OSVERSIONINFOW os{}; os.dwOSVersionInfoSize=sizeof(os);
        if (rtl && rtl(&os)==0) Log("ENV","Windows=%lu.%lu build=%lu",
            static_cast<unsigned long>(os.dwMajorVersion),static_cast<unsigned long>(os.dwMinorVersion),
            static_cast<unsigned long>(os.dwBuildNumber));
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,GetCurrentProcessId());
        if (snap != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W e{}; e.dwSize=sizeof(e);
            if (Module32FirstW(snap,&e)) do {
                auto name=Utf8(e.szModule);
                // File basenames and versions only; do not emit absolute user paths.
                auto version=Version(e.szExePath);
                Log("MODULE","name=%s version=%s image=%p size=%lu",name.c_str(),version.c_str(),
                    static_cast<void*>(e.modBaseAddr),static_cast<unsigned long>(e.modBaseSize));
            } while (Module32NextW(snap,&e));
            CloseHandle(snap);
        } else Log("ENV","module snapshot failed error=%lu",GetLastError());
        if (!gameRoot.empty()) {
            for (auto relative : {L"archive\\pc\\mod",L"red4ext\\plugins",L"r6\\scripts",L"r6\\tweaks",L"mods"}) {
                auto dir=gameRoot/relative;
                std::error_code ec;
                size_t count=0;
                std::filesystem::recursive_directory_iterator it(dir,
                    std::filesystem::directory_options::skip_permission_denied,ec),end;
                while (!ec && it!=end && count<20000) {
                    if (it.depth()>6 || it->is_symlink(ec)) it.disable_recursion_pending();
                    if (ec) break;
                    if (it->is_regular_file(ec)) {
                        auto extension=it->path().extension().wstring();
                        if (extension==L".archive" || extension==L".xl" || extension==L".dll" ||
                            extension==L".reds" || extension==L".yaml" || extension==L".yml" ||
                            extension==L".tweak" || extension==L".xlz") {
                            auto filename=it->path().lexically_relative(gameRoot).wstring();
                            auto size=it->file_size(ec);
                            Log("INSTALLED","%s bytes=%llu",Utf8(filename.c_str()).c_str(),
                                static_cast<unsigned long long>(ec?0:size));
                            ++count;
                        }
                    }
                    if (ec) break;
                    it.increment(ec);
                }
                Log("ENV","inventory folder=%s files=%zu error=%d",Utf8(relative).c_str(),count,ec.value());
            }
        }
        Log("ENV","end stage=%s",stage);
    } catch (...) { Log("ENV","inventory interrupted stage=%s",stage); }
}
}
