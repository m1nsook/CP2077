#pragma once

#include <RED4ext/Api/v1/PluginHandle.hpp>
#include <RED4ext/Api/v1/Sdk.hpp>

#include <Windows.h>
#include <cstdint>

struct IDXGISwapChain4;
struct ID3D12CommandQueue;

namespace m1core {

// RED4ext's exported address resolver returns 0 for an unknown game symbol.
// We deliberately avoid UniversalRelocPtr here: its SDK implementation may
// terminate the entire game process when a hash is missing.
[[nodiscard]] std::uintptr_t ResolveGameSymbol(std::uint32_t hash) noexcept;

// The game owns the swapchain, device and command queue. These are borrowed
// references valid only during a present callback, never retained blindly.
struct GameFrame {
    IDXGISwapChain4* swapchain = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    HWND window = nullptr;
};
[[nodiscard]] GameFrame DiscoverGameFrame(std::int32_t deviceIndex) noexcept;

class RenderBridge {
public:
    bool Install(RED4ext::v1::PluginHandle handle,
                 const RED4ext::v1::Sdk* sdk) noexcept;
    void Uninstall() noexcept;
    [[nodiscard]] bool Installed() const noexcept { return presentAddress_ != nullptr; }
    static RenderBridge& Get() noexcept;
private:
    using PresentFn = void* (*)(std::int32_t*,std::uint8_t,UINT);
    using ResizeFn = void* (*)(std::uint32_t,std::uint32_t,std::uint32_t,std::uint8_t,std::int32_t*);
    using ShutdownFn = void* (*)(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
    static void* OnPresent(std::int32_t*,std::uint8_t,UINT);
    static void* OnResize(std::uint32_t,std::uint32_t,std::uint32_t,std::uint8_t,std::int32_t*);
    static void* OnShutdown(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);

    RED4ext::v1::PluginHandle handle_{};
    const RED4ext::v1::Sdk* sdk_ = nullptr;
    void* presentAddress_ = nullptr;
    void* resizeAddress_ = nullptr;
    void* shutdownAddress_ = nullptr;
    PresentFn originalPresent_ = nullptr;
    ResizeFn originalResize_ = nullptr;
    ShutdownFn originalShutdown_ = nullptr;
};
} // namespace m1core
