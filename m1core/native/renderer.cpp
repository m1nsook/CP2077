#include "renderer.hpp"
#include "ui.hpp"
#include "input.hpp"
#include "game_lifecycle.hpp"
#include "ImGuiCustomTheme.h"

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include <algorithm>
#include <cfloat>
#include <cstring>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND window,UINT message,WPARAM wparam,LPARAM lparam);

namespace m1core {
namespace {
constexpr UINT kShaderDescriptors = 32;
constexpr DWORD kGpuResetTimeoutMs = 1000;
constexpr DWORD kInputStallTimeoutMs = 2000;
constexpr std::size_t kMaxQueuedInput = 1024;

bool LegacyKeyboard(UINT message) noexcept
{
    return message >= WM_KEYFIRST && message <= WM_KEYLAST;
}
bool LegacyButton(UINT message) noexcept
{
    return message >= WM_LBUTTONDOWN && message <= WM_XBUTTONDBLCLK;
}
bool LegacyWheel(UINT message) noexcept
{
    return message == WM_MOUSEWHEEL || message == WM_MOUSEHWHEEL;
}


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

    // Retain UI window state across ordinary swapchain resize, as CET does.
    // The context is destroyed only on actual plugin/game shutdown.
    if (!ownsContext_) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ownsContext_ = true;
        ImGui::StyleColorsDark();
        m1CoreTheme(ImGui::GetStyle()); // User-supplied ImGui style, no custom controls.
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange |
                                     ImGuiConfigFlags_NavEnableKeyboard;
    }

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
    RenderBridge::Get().LogInfo("m1core: ImGui Win32/DX12 backends initialized");
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
    if (!IsWindow(source.window))
        return;
    // Initialize graphics on Present independently of window focus; only
    // drawing and input ownership need the foreground game HWND.
    // Initialize on the game's Present callback even with the menu hidden,
    // but never trap input just because a graphics context exists.
    if (!initialized_ && !Initialize(source)) return;
    if (GameBaseInitialized())
        InputRouter::Get().ArmFirstRun();
    if (source.swapchain != swapchain_.Get() || source.queue != queue_.Get() ||
        source.window != gameWindow_) {
        Reset();
        return;
    }
    if (!InputRouter::Get().ShouldDraw() &&
        !InputRouter::Get().NeedsPopupDismiss()) return;
    const UINT index = swapchain_->GetCurrentBackBufferIndex();
    if (index >= buffers_.size()) return;
    auto& frame = buffers_[index];
    if (!FrameReady(frame)) {
        if (inputReady_.load(std::memory_order_acquire) &&
            lastSubmittedAt_ && GetTickCount64() - lastSubmittedAt_ >
                                  kInputStallTimeoutMs) {
            // Stalled GPU must never permanently trap game controls.
            inputReady_.store(false, std::memory_order_release);
            InputRouter::Get().Close();
        }
        return;
    }

    // Both backend NewFrame calls and all ImGui UI drawing stay on the
    // renderer thread; Win32 messages are forwarded through the same mutex.
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();

    // The game's render target can differ from the HWND's physical size.
    // Match the actual D3D12 backbuffer before ImGui builds click targets.
    const auto resource = frame.backbuffer->GetDesc();
    auto& io = ImGui::GetIO();
    if (resource.Width && resource.Height && resource.Width <= 16384 &&
        resource.Height <= 16384 &&
        io.DisplaySize.x > 0 && io.DisplaySize.y > 0 &&
        (io.DisplaySize.x != static_cast<float>(resource.Width) ||
         io.DisplaySize.y != static_cast<float>(resource.Height))) {
        POINT cursor{};
        if (GetCursorPos(&cursor) && ScreenToClient(gameWindow_,&cursor)) {
            io.AddMousePosEvent(
                static_cast<float>(cursor.x) * static_cast<float>(resource.Width) / io.DisplaySize.x,
                static_cast<float>(cursor.y) * static_cast<float>(resource.Height) / io.DisplaySize.y);
        } else {
            io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);
        }
        io.DisplaySize = ImVec2(static_cast<float>(resource.Width),
                                 static_cast<float>(resource.Height));
    }
    // Never call ImGui from WndProc: process queued raw + legacy messages on
    // the same render thread used by ImGui::NewFrame().
    DrainInput();
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
    if (SUCCEEDED(queue_->Signal(fence_.Get(),++nextFence_))) {
        frame.submittedFence = nextFence_;
        lastSubmittedAt_ = GetTickCount64();
        // First successful submitted frame enables HWND capture and the
        // REDengine cursor. The notification is delivered on its own thread.
        if (!inputReady_.exchange(true, std::memory_order_acq_rel)) {
            RenderBridge::Get().LogInfo("m1core: first ImGui frame submitted");
            InputRouter::Get().RequestCursorSync();
        }
        InputRouter::Get().RetryCursorSync();
    } else {
        // A failed GPU submission must not strand keyboard or mouse input.
        inputReady_.store(false, std::memory_order_release);
        InputRouter::Get().Close();
    }
}

LRESULT Renderer::ForwardInput(HWND window,UINT message,
                               WPARAM wparam,LPARAM lparam) noexcept
{
    if (!initialized_ || window != gameWindow_ ||
        GetForegroundWindow() != gameWindow_) return 0;
    std::lock_guard lock(inputMutex_);
    if (inputPackets_.size() >= kMaxQueuedInput) inputPackets_.clear();
    InputPacket packet{};
    packet.window = window;
    packet.message = message;
    packet.wparam = wparam;
    packet.lparam = lparam;
    inputPackets_.push_back(packet);
    return 1;
}

void Renderer::ForwardRawInput(HWND window,const RAWINPUT& raw) noexcept
{
    if (!initialized_ || window != gameWindow_ ||
        GetForegroundWindow() != gameWindow_) return;
    if (raw.header.dwType != RIM_TYPEKEYBOARD &&
        raw.header.dwType != RIM_TYPEMOUSE) return;

    InputPacket packet{};
    packet.window = window;
    packet.raw = true;
    packet.rawType = raw.header.dwType;
    if (packet.rawType == RIM_TYPEKEYBOARD) {
        packet.keyboard = raw.data.keyboard;
        // Cyberpunk may register RIDEV_NOLEGACY and never send WM_CHAR.
        // Translate text on the real HWND thread before keyboard state moves.
        const auto& kb = packet.keyboard;
        if ((kb.Flags & RI_KEY_BREAK) == 0 &&
            kb.VKey >= VK_SPACE && kb.VKey < 255 &&
            !(GetKeyState(VK_CONTROL) & 0x8000) &&
            !(GetKeyState(VK_MENU) & 0x8000)) {
            BYTE state[256]{};
            if (GetKeyboardState(state)) {
                state[kb.VKey] = 0x80;
                const auto thread = GetWindowThreadProcessId(window,nullptr);
                const int chars = ToUnicodeEx(kb.VKey,kb.MakeCode,state,
                    packet.text,8,4,GetKeyboardLayout(thread));
                if (chars > 0)
                    packet.textLength = static_cast<unsigned>(std::min(chars,8));
            }
        }
    } else {
        packet.mouse = raw.data.mouse;
    }
    std::lock_guard lock(inputMutex_);
    if (inputPackets_.size() >= kMaxQueuedInput) inputPackets_.clear();
    inputPackets_.push_back(packet);
}

void Renderer::DrainInput() noexcept
{
    std::vector<InputPacket> pending;
    {
        std::lock_guard lock(inputMutex_);
        pending.swap(inputPackets_);
    }
    auto& io = ImGui::GetIO();
    const int focus = pendingFocus_.exchange(0,std::memory_order_acq_rel);
    if (focus != 0) io.AddFocusEvent(focus > 0);
    if (pending.empty()) return;

    // The game may emit BOTH WM_INPUT and legacy messages for one click.
    // Prefer legacy when present and synthesize only the missing raw events.
    bool legacyKeys = false,legacyButtons = false,legacyWheel = false;
    for (const auto& e : pending) {
        if (e.raw) continue;
        legacyKeys |= LegacyKeyboard(e.message);
        legacyButtons |= LegacyButton(e.message);
        legacyWheel |= LegacyWheel(e.message);
        ImGui_ImplWin32_WndProcHandler(e.window,e.message,e.wparam,e.lparam);
    }

    for (const auto& e : pending) {
        if (!e.raw) continue;
        if (e.rawType == RIM_TYPEKEYBOARD && !legacyKeys) {
            const auto& kb = e.keyboard;
            if (kb.VKey == 0 || kb.VKey >= 255) continue;
            const bool released = (kb.Flags & RI_KEY_BREAK) != 0;
            LPARAM data = static_cast<LPARAM>(kb.MakeCode) << 16;
            if (kb.Flags & RI_KEY_E0) data |= static_cast<LPARAM>(1) << 24;
            if (released) data |= static_cast<LPARAM>(1) << 31;
            ImGui_ImplWin32_WndProcHandler(e.window,
                released ? WM_KEYUP : WM_KEYDOWN,kb.VKey,data);
            if (!released)
                for (unsigned i=0;i<e.textLength;++i)
                    io.AddInputCharacterUTF16(e.text[i]);
        } else if (e.rawType == RIM_TYPEMOUSE) {
            const auto& m = e.mouse;
            if (!legacyButtons) {
                constexpr struct {USHORT down,up;int button;} mapping[]={
                    {RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_LEFT_BUTTON_UP,0},
                    {RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_UP,1},
                    {RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_UP,2},
                    {RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_4_UP,3},
                    {RI_MOUSE_BUTTON_5_DOWN,RI_MOUSE_BUTTON_5_UP,4}
                };
                for (const auto& b:mapping) {
                    if (m.usButtonFlags & b.down) io.AddMouseButtonEvent(b.button,true);
                    if (m.usButtonFlags & b.up) io.AddMouseButtonEvent(b.button,false);
                }
            }
            if (!legacyWheel) {
                if (m.usButtonFlags & RI_MOUSE_WHEEL)
                    io.AddMouseWheelEvent(0.0f,
                        static_cast<float>(static_cast<SHORT>(m.usButtonData)) / WHEEL_DELTA);
                if (m.usButtonFlags & RI_MOUSE_HWHEEL)
                    io.AddMouseWheelEvent(
                        static_cast<float>(static_cast<SHORT>(m.usButtonData)) / WHEEL_DELTA,0.0f);
            }
        }
    }
}

void Renderer::CancelInput() noexcept
{
    // Never mutate ImGui IO from the HWND thread.
    pendingFocus_.store(-1,std::memory_order_release);
    std::lock_guard lock(inputMutex_);
    inputPackets_.clear();
}

void Renderer::ResumeInput() noexcept
{
    pendingFocus_.store(1,std::memory_order_release);
}

bool Renderer::FlushGpu() noexcept
{
    if (!queue_ || !fence_ || !fenceEvent_) return true;
    const auto required = ++nextFence_;
    if (FAILED(queue_->Signal(fence_.Get(),required)) ||
        FAILED(fence_->SetEventOnCompletion(required,fenceEvent_))) return false;
    return WaitForSingleObject(fenceEvent_,kGpuResetTimeoutMs) == WAIT_OBJECT_0;
}

void Renderer::Release(bool destroyContext) noexcept
{
    CancelInput();
    // Fail open immediately, even if GPU teardown must wait.
    if (inputReady_.exchange(false, std::memory_order_acq_rel))
        InputRouter::Get().RequestCursorSync();
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
    if (ownsContext_ && destroyContext) {
        ImGui::DestroyContext();
        ownsContext_ = false;
    }
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
    lastSubmittedAt_ = 0;
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
    std::lock_guard lock(mutex_);
    Release(true);
}
} // namespace m1core
