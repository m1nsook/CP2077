#pragma once

#include "render_bridge.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

namespace m1core {
class Renderer {
public:
    static Renderer& Get() noexcept;
    void Present(const GameFrame& frame) noexcept;
    void Reset() noexcept;
    void Shutdown() noexcept;
    // Called from the game window's WndProc on the HWND thread.
    [[nodiscard]] LRESULT ForwardInput(HWND window,UINT message,
                                       WPARAM wparam,LPARAM lparam) noexcept;
    void CancelInput() noexcept;
    void ResumeInput() noexcept;
    // True only after a complete overlay command list was submitted and
    // signalled. The HWND router must never swallow input before this.
    [[nodiscard]] bool AcceptingInput() const noexcept {
        return inputReady_.load(std::memory_order_acquire);
    }
private:
    using ComPtrFence = Microsoft::WRL::ComPtr<ID3D12Fence>;
    struct FrameBuffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> backbuffer;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        D3D12_CPU_DESCRIPTOR_HANDLE target{};
        std::uint64_t submittedFence = 0;
    };
    bool Initialize(const GameFrame& frame) noexcept;
    void Release() noexcept;
    [[nodiscard]] bool FrameReady(FrameBuffer& frame) noexcept;
    [[nodiscard]] bool FlushGpu() noexcept;

    std::recursive_mutex mutex_;
    std::atomic_bool inputReady_{false};
    HWND gameWindow_ = nullptr;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> renderTargets_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shaderResources_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtrFence fence_;
    HANDLE fenceEvent_ = nullptr;
    std::uint64_t nextFence_ = 0;
    UINT descriptorIncrement_ = 0;
    UINT resourceIncrement_ = 0;
    std::vector<FrameBuffer> buffers_;
    bool imguiWin32_ = false;
    bool imguiDx12_ = false;
    bool ownsContext_ = false;
    bool initialized_ = false;
};
} // namespace m1core
