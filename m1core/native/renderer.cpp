#include "renderer.hpp"
#include "ui.hpp"
#include "input.hpp"

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include <algorithm>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window,UINT message,WPARAM wparam,LPARAM lparam);

namespace m1core {
namespace {
constexpr UINT kShaderDescriptors = 32;
constexpr DWORD kGpuResetTimeoutMs = 1000;

void AllocateDescriptor(ImGui_ImplDX12_InitInfo* info,
                        D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                        D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
{
    // Our first implementation uses only a font atlas, so one descriptor is
    // sufficient. No game texture SRVs are stolen from the host.
    auto* renderer = static_cast<Renderer*>(info->UserData);
    const auto descriptor = info->SrvDescriptorHeap;
    if (!renderer || !descriptor || !cpu || !gpu) return;
    *cpu = descriptor->GetCPUDescriptorHandleForHeapStart();
    *gpu = descriptor->GetGPUDescriptorHandleForHeapStart();
}
void FreeDescriptor(ImGui_ImplDX12_InitInfo*,
                    D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_GPU_DESCRIPTOR_HANDLE)
{
    // Single font texture descriptor is released with its private heap.
}
}

Renderer& Renderer::Get() noexcept
{
    static Renderer renderer;
    return renderer;
}

bool Renderer::Initialize(const GameFrame& source) noexcept
{
    if (initialized_) return true;
    Release();
    if (!source.swapchain || !source.queue || !IsWindow(source.window))
        return false;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(source.swapchain->GetDesc(&desc)) ||
        desc.BufferCount == 0 || desc.BufferCount > 4)
        return false;

    gameWindow_ = source.window;
    swapchain_ = source.swapchain;
    queue_ = source.queue;
    if (FAILED(source.swapchain->GetDevice(IID_PPV_ARGS(&device_))) || !device_)
        { Release(); return false; }
    Microsoft::WRL::ComPtr<ID3D12Device> queueDevice;
    if (FAILED(source.queue->GetDevice(IID_PPV_ARGS(&queueDevice))) ||
        queueDevice.Get() != device_.Get())
        { Release(); return false; }

    D3D12_DESCRIPTOR_HEAP_DESC targetDesc{};
    targetDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    targetDesc.NumDescriptors = desc.BufferCount;
    if (FAILED(device_->CreateDescriptorHeap(&targetDesc,
        IID_PPV_ARGS(&renderTargets_)))) { Release(); return false; }
    descriptorIncrement_ =
        device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC resourceDesc{};
    resourceDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    resourceDesc.NumDescriptors = kShaderDescriptors;
    resourceDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device_->CreateDescriptorHeap(&resourceDesc,
        IID_PPV_ARGS(&shaderResources_)))) { Release(); return false; }

    buffers_.resize(desc.BufferCount);
    const auto targetStart = renderTargets_->GetCPUDescriptorHandleForHeapStart();
    DXGI_FORMAT rtvFormat = DXGI_FORMAT_UNKNOWN;
    for (UINT i = 0; i < desc.BufferCount; ++i) {
        auto& b = buffers_[i];
        if (FAILED(swapchain_->GetBuffer(i,IID_PPV_ARGS(&b.backbuffer))) ||
            FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                        IID_PPV_ARGS(&b.allocator)))) { Release(); return false; }
        if (i == 0) rtvFormat = b.backbuffer->GetDesc().Format;
        b.target = targetStart;
        b.target.ptr += static_cast<SIZE_T>(i) * descriptorIncrement_;
        device_->CreateRenderTargetView(b.backbuffer.Get(),nullptr,b.target);
    }
    if (rtvFormat == DXGI_FORMAT_UNKNOWN) { Release(); return false; }

    if (FAILED(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
        buffers_.front().allocator.Get(),nullptr,IID_PPV_ARGS(&commandList_))) ||
        FAILED(commandList_->Close())) { Release(); return false; }
    if (FAILED(device_->CreateFence(0,D3D12_FENCE_FLAG_NONE,
                                   IID_PPV_ARGS(&fence_)))) { Release(); return false; }
    fenceEvent_ = CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if (!fenceEvent_) { Release(); return false; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ownsContext_ = true;
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 0.0F;
    style.ChildRounding = 0.0F;
    style.FrameRounding = 0.0F;
    style.PopupRounding = 0.0F;
    style.FramePadding = ImVec2(10.0F,7.0F);
    style.ItemSpacing = ImVec2(10.0F,9.0F);
    ImGui::GetIO().MouseDrawCursor = false; // Always one: engine's real cursor
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    if (!ImGui_ImplWin32_Init(gameWindow_)) { Release(); return false; }
    imguiWin32_ = true;

    ImGui_ImplDX12_InitInfo init{};
    init.Device = device_.Get();
    init.CommandQueue = queue_.Get();
    init.NumFramesInFlight = static_cast<int>(desc.BufferCount);
    init.RTVFormat = rtvFormat;
    init.SrvDescriptorHeap = shaderResources_.Get();
    init.UserData = this;
    init.SrvDescriptorAllocFn = AllocateDescriptor;
    init.SrvDescriptorFreeFn = FreeDescriptor;
    if (!ImGui_ImplDX12_Init(&init)) { Release(); return false; }
    imguiDx12_ = true;

    if (!InputRouter::Get().Ready()) {
        Release();
        return false;
    }
    initialized_ = true;
    return true;
}

bool Renderer::FrameReady(FrameBuffer& frame) noexcept
{
    if (!frame.submittedFence) return true;
    if (!fence_) return false;
    // Never block the game's Present for another process/window; skip a
    // frame while the GPU is busy instead of resetting an in-flight allocator.
    return fence_->GetCompletedValue() >= frame.submittedFence;
}

void Renderer::Present(const GameFrame& source) noexcept
{
    std::lock_guard lock(mutex_);
    if (!IsWindow(source.window) || GetForegroundWindow() != source.window)
        return;
    if (!initialized_ && !Initialize(source)) return;
    if (source.swapchain != swapchain_.Get() || source.queue != queue_.Get() ||
        source.window != gameWindow_) {
        Reset();
        return;
    }
    const UINT index = swapchain_->GetCurrentBackBufferIndex();
    if (index >= buffers_.size()) return;
    auto& frame = buffers_[index];
    if (!FrameReady(frame)) return;

    // Both backend NewFrame calls and all ImGui UI drawing stay on the
    // renderer thread; Win32 messages are forwarded through the same mutex.
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    DrawNativeSettingsUI();
    ImGui::Render();
    auto* draw = ImGui::GetDrawData();
    if (!draw || draw->TotalVtxCount <= 0) return;

    if (FAILED(frame.allocator->Reset())) return;
    if (FAILED(commandList_->Reset(frame.allocator.Get(),nullptr))) return;
    D3D12_RESOURCE_BARRIER transition{};
    transition.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transition.Transition.pResource = frame.backbuffer.Get();
    transition.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    transition.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    transition.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1,&transition);
    ID3D12DescriptorHeap* heaps[] = {shaderResources_.Get()};
    commandList_->SetDescriptorHeaps(1,heaps);
    commandList_->OMSetRenderTargets(1,&frame.target,FALSE,nullptr);
    ImGui_ImplDX12_RenderDrawData(draw,commandList_.Get());
    std::swap(transition.Transition.StateBefore,transition.Transition.StateAfter);
    commandList_->ResourceBarrier(1,&transition);
    if (FAILED(commandList_->Close())) return;
    ID3D12CommandList* commands[] = {commandList_.Get()};
    queue_->ExecuteCommandLists(1,commands);
    if (SUCCEEDED(queue_->Signal(fence_.Get(),++nextFence_)))
        frame.submittedFence = nextFence_;
}

LRESULT Renderer::ForwardInput(HWND window,UINT message,
                               WPARAM wparam,LPARAM lparam) noexcept
{
    std::lock_guard lock(mutex_);
    if (!initialized_ || window != gameWindow_ ||
        GetForegroundWindow() != gameWindow_) return 0;
    return ImGui_ImplWin32_WndProcHandler(window,message,wparam,lparam);
}

void Renderer::CancelInput() noexcept
{
    std::lock_guard lock(mutex_);
    if (initialized_ && ImGui::GetCurrentContext())
        ImGui::GetIO().AddFocusEvent(false);
}

bool Renderer::FlushGpu() noexcept
{
    if (!queue_ || !fence_ || !fenceEvent_) return true;
    const auto required = ++nextFence_;
    if (FAILED(queue_->Signal(fence_.Get(),required)) ||
        FAILED(fence_->SetEventOnCompletion(required,fenceEvent_))) return false;
    return WaitForSingleObject(fenceEvent_,kGpuResetTimeoutMs) == WAIT_OBJECT_0;
}

void Renderer::Release() noexcept
{
    // The passive input listener survives a resize, allowing user hotkeys
    // to work after device reinitialization without hooking the HWND again.
    // The bridge uninstalls it on actual game/plugin shutdown.
    // The renderer must not release an in-flight allocator when GPU work
    // still references it. Game resize/shutdown normally drains the queue;
    // a bounded wait prevents a permanent process hang if it does not.
    const bool completed = FlushGpu();
    if (!completed && initialized_) {
        // On a stalled GPU, keep COM resources alive until process cleanup.
        // This is preferable to releasing memory still in use by the game.
    }
    if (imguiDx12_) { ImGui_ImplDX12_Shutdown(); imguiDx12_ = false; }
    if (imguiWin32_) { ImGui_ImplWin32_Shutdown(); imguiWin32_ = false; }
    if (ownsContext_) { ImGui::DestroyContext(); ownsContext_ = false; }
    buffers_.clear();
    commandList_.Reset();
    renderTargets_.Reset();
    shaderResources_.Reset();
    fence_.Reset();
    device_.Reset();
    queue_.Reset();
    swapchain_.Reset();
    if (fenceEvent_) { CloseHandle(fenceEvent_); fenceEvent_ = nullptr; }
    descriptorIncrement_ = 0;
    resourceIncrement_ = 0;
    nextFence_ = 0;
    gameWindow_ = nullptr;
    initialized_ = false;
}

void Renderer::Reset() noexcept
{
    std::lock_guard lock(mutex_);
    Release();
}

void Renderer::Shutdown() noexcept
{
    Reset();
}
} // namespace m1core
