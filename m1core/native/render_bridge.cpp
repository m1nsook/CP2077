#include "render_bridge.hpp"
#include "renderer.hpp"

#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstddef>
#include <cstring>
#include <limits>

namespace m1core {
namespace {
// Stable names from the public RED4ext address database, not raw executable
// offsets. The resolver is checked for null before installing any hooks.
constexpr std::uint32_t kGpuPresent = 2468877568u;
constexpr std::uint32_t kGpuResize = 239671909u;
constexpr std::uint32_t kGpuShutdown = 3192982283u;
constexpr std::uint32_t kRenderContext = 1239944840u;

// Game-render structure layout used by the current engine; isolate it here
// rather than trusting an SDK-internal C++ struct across future versions.
constexpr std::size_t kDeviceBase = 0xCEBF48;
constexpr std::size_t kDeviceStride = 0xB0;
constexpr std::size_t kDirectQueueOffset = 0x14104D0;
constexpr std::int32_t kMaxDeviceIndex = 48;

[[nodiscard]] bool Readable(const void* address, std::size_t length) noexcept
{
    if (!address || !length) return false;
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(address,&region,sizeof(region)) != sizeof(region))
        return false;
    if (region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
        return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto limit = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
    return begin < limit && length <= limit-begin;
}

template<typename T>
[[nodiscard]] T Read(const void* address) noexcept
{
    T result{};
    if (Readable(address,sizeof(T))) std::memcpy(&result,address,sizeof(T));
    return result;
}
} // namespace

std::uintptr_t ResolveGameSymbol(std::uint32_t hash) noexcept
{
    using ResolverFn = std::uintptr_t (*)(std::uint32_t);
    const auto red4ext = GetModuleHandleW(L"RED4ext.dll");
    if (!red4ext) return 0;
    const auto resolver = reinterpret_cast<ResolverFn>(
        GetProcAddress(red4ext,"RED4ext_ResolveAddress"));
    return resolver ? resolver(hash) : 0;
}

GameFrame DiscoverGameFrame(std::int32_t deviceIndex) noexcept
{
    GameFrame frame{};
    if (deviceIndex <= 0 || deviceIndex > kMaxDeviceIndex) return frame;
    const auto location = ResolveGameSymbol(kRenderContext);
    const auto context = Read<std::uintptr_t>(reinterpret_cast<void*>(location));
    if (context == 0) return frame;
    const auto queue = Read<ID3D12CommandQueue*>(
        reinterpret_cast<const void*>(context+kDirectQueueOffset));
    const auto swapchain = Read<IDXGISwapChain4*>(
        reinterpret_cast<const void*>(context+kDeviceBase+
            (static_cast<std::size_t>(deviceIndex-1)*kDeviceStride)));
    if (!queue || !swapchain) return frame;

    // The game, not our plugin, controls the DX12 output. Check its HWND and
    // device identity before the renderer ever reads a backbuffer.
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapchain->GetDesc(&desc)) || !IsWindow(desc.OutputWindow))
        return frame;
    frame.swapchain = swapchain;
    frame.queue = queue;
    frame.window = desc.OutputWindow;
    return frame;
}

RenderBridge& RenderBridge::Get() noexcept
{
    static RenderBridge instance;
    return instance;
}

bool RenderBridge::Install(RED4ext::v1::PluginHandle handle,
                           const RED4ext::v1::Sdk* sdk) noexcept
{
    if (Installed()) return true;
    if (!sdk || !sdk->hooking || !sdk->hooking->Attach ||
        !sdk->hooking->Detach) return false;
    const auto present = ResolveGameSymbol(kGpuPresent);
    const auto resize = ResolveGameSymbol(kGpuResize);
    const auto shutdown = ResolveGameSymbol(kGpuShutdown);
    if (!present || !resize || !shutdown) return false;

    handle_ = handle;
    sdk_ = sdk;
    presentAddress_ = reinterpret_cast<void*>(present);
    resizeAddress_ = reinterpret_cast<void*>(resize);
    shutdownAddress_ = reinterpret_cast<void*>(shutdown);

    if (!sdk_->hooking->Attach(handle_,presentAddress_,reinterpret_cast<void*>(&OnPresent),
                              reinterpret_cast<void**>(&originalPresent_))) {
        Uninstall();
        return false;
    }
    if (!sdk_->hooking->Attach(handle_,resizeAddress_,reinterpret_cast<void*>(&OnResize),
                              reinterpret_cast<void**>(&originalResize_))) {
        Uninstall();
        return false;
    }
    if (!sdk_->hooking->Attach(handle_,shutdownAddress_,reinterpret_cast<void*>(&OnShutdown),
                              reinterpret_cast<void**>(&originalShutdown_))) {
        Uninstall();
        return false;
    }
    return true;
}

void RenderBridge::Uninstall() noexcept
{
    Renderer::Get().Shutdown();
    if (sdk_ && sdk_->hooking && sdk_->hooking->Detach) {
        if (shutdownAddress_ && originalShutdown_)
            sdk_->hooking->Detach(handle_,shutdownAddress_);
        if (resizeAddress_ && originalResize_)
            sdk_->hooking->Detach(handle_,resizeAddress_);
        if (presentAddress_ && originalPresent_)
            sdk_->hooking->Detach(handle_,presentAddress_);
    }
    originalPresent_ = nullptr;
    originalResize_ = nullptr;
    originalShutdown_ = nullptr;
    presentAddress_ = nullptr;
    resizeAddress_ = nullptr;
    shutdownAddress_ = nullptr;
    sdk_ = nullptr;
}

void* RenderBridge::OnPresent(std::int32_t* deviceIndex,
                              std::uint8_t sync, UINT interval)
{
    auto& bridge = Get();
    if (deviceIndex && bridge.originalPresent_) {
        const auto frame = DiscoverGameFrame(*deviceIndex);
        if (frame.window && GetForegroundWindow() == frame.window)
            Renderer::Get().Present(frame);
    }
    return bridge.originalPresent_ ?
        bridge.originalPresent_(deviceIndex,sync,interval) : nullptr;
}

void* RenderBridge::OnResize(std::uint32_t width,std::uint32_t height,
                             std::uint32_t other,std::uint8_t mode,
                             std::int32_t* deviceIndex)
{
    auto& bridge = Get();
    Renderer::Get().Reset();
    return bridge.originalResize_ ?
        bridge.originalResize_(width,height,other,mode,deviceIndex) : nullptr;
}

void* RenderBridge::OnShutdown(std::uint64_t a,std::uint64_t b,
                               std::uint64_t c,std::uint64_t d)
{
    auto& bridge = Get();
    Renderer::Get().Shutdown();
    return bridge.originalShutdown_ ?
        bridge.originalShutdown_(a,b,c,d) : nullptr;
}
} // namespace m1core
