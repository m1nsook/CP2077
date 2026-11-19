#include "overlay.hpp"
#include "input_gate.hpp"
#include "cursor.hpp"
#include "raw_input.hpp"
#include "settings.hpp"
#include "ui.hpp"

#include <RED4ext/RED4ext.hpp>
#include <RED4ext/GpuApi/DeviceData.hpp>

#include <Windows.h>
#include <commctrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <imgui.h>
#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

// The ImGui Win32 backend intentionally leaves this declaration to callers.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace m1core {
namespace {

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using Resize1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT,
                                              const UINT*, IUnknown* const*);

constexpr UINT_PTR kSubclassId = 0x4D31434F;
constexpr UINT kMaxBuffers = 8;
constexpr UINT kSrvDescriptors = 64;

struct Frame {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12Resource> buffer;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 pendingFence = 0;
};

struct Graphics {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    UINT64 nextFence = 0;
    std::vector<Frame> frames;
    std::array<bool, kSrvDescriptors> descriptors{};
    UINT descriptorStride = 0;
    bool contextCreated = false;
    bool win32Ready = false;
    bool dx12Ready = false;
    bool ready = false;
};

std::mutex g_stateMutex;
std::mutex g_renderMutex;
InputGate g_gate;
Graphics g_graphics;
Registry* g_registry = nullptr;
RED4ext::v1::PluginHandle g_handle{};
const RED4ext::v1::Sdk* g_sdk = nullptr;
HWND g_window = nullptr;
IDXGISwapChain4* g_selectedChain = nullptr; // Owned by Cyberpunk.
std::vector<MSG> g_messages;
std::vector<RawSample> g_rawSamples;
RawInputBridge g_rawBridge;
bool g_wasCapturingLastFrame = false;
UINT g_pendingFrames = 0;
std::atomic_bool g_enabled{false};
std::atomic_bool g_renderError{false};
bool g_hooked = false;
bool g_subclassed = false;
bool g_loggedThreadMismatch = false;
bool g_f11WasDown = false;
bool g_escapeWasDown = false;
bool g_emergencyWasDown = false;
PresentFn g_present = nullptr;
ResizeFn g_resize = nullptr;
Resize1Fn g_resize1 = nullptr;
void* g_presentTarget = nullptr;
void* g_resizeTarget = nullptr;
void* g_resize1Target = nullptr;

void Log(const char* message)
{
    if (g_sdk && g_sdk->logger)
        g_sdk->logger->Info(g_handle, message);
}
void Warn(const char* message)
{
    if (g_sdk && g_sdk->logger)
        g_sdk->logger->Warn(g_handle, message);
}

bool IsFocused()
{
    return g_window && IsWindow(g_window) &&
           (GetForegroundWindow() == g_window || GetActiveWindow() == g_window);
}

void CloseOverlay()
{
    {
        std::lock_guard lock(g_stateMutex);
        g_gate.Close();
        g_messages.clear();
        g_rawSamples.clear();
    }
    SetCursorCaptured(false);
}

void EmergencyDisable()
{
    g_enabled.store(false);
    CloseOverlay();
    Warn("m1core overlay: emergency disabled; delete overlay.enable to keep it off next launch");
}

bool IsKeyboardMessage(UINT msg)
{
    switch (msg) {
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
    case WM_CHAR: case WM_SYSCHAR: case WM_DEADCHAR: case WM_SYSDEADCHAR:
        return true;
    default: return false;
    }
}

bool IsMouseMessage(UINT msg)
{
    switch (msg) {
    case WM_MOUSEMOVE: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_SETCURSOR:
        return true;
    default: return false;
    }
}

LRESULT CALLBACK WindowSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                 UINT_PTR, DWORD_PTR)
{
    if (message == WM_NCDESTROY) {
        CloseOverlay();
        RemoveWindowSubclass(window, WindowSubclass, kSubclassId);
        g_subclassed = false;
        g_window = nullptr;
        return DefSubclassProc(window, message, wparam, lparam);
    }

    if (message == WM_KILLFOCUS || (message == WM_ACTIVATEAPP && !wparam)) {
        std::lock_guard lock(g_stateMutex);
        g_gate.SetGameFocused(false);
        g_messages.clear();
        g_rawSamples.clear();
        // The OS cursor must be freed/restored without holding g_stateMutex.
    }
    if (message == WM_KILLFOCUS || (message == WM_ACTIVATEAPP && !wparam))
        SetCursorCaptured(false);

    if (g_enabled.load() && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
        && wparam == VK_F11
        && (GetAsyncKeyState(VK_CONTROL) & 0x8000)
        && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        EmergencyDisable();
        return 0;
    }

    // F11 is polled in the Present callback, because the game may use RAWINPUT
    // (RIDEV_NOLEGACY) and never produce WM_KEYDOWN for the hotkey.
    if (g_enabled.load() && IsKeyboardMessage(message) && wparam == VK_F11)
        return 0;

    bool capture = false;
    if (g_enabled.load()) {
        std::lock_guard lock(g_stateMutex);
        capture = g_gate.MayCaptureInput();
        if (capture && (IsKeyboardMessage(message) || IsMouseMessage(message))) {
            // Win32 backend receives events on the render thread: never call
            // ImGui from this window thread, where IO would race with NewFrame().
            if (g_messages.size() < 512) {
                MSG event{};
                event.hwnd = window;
                event.message = message;
                event.wParam = wparam;
                event.lParam = lparam;
                g_messages.push_back(event);
            }
        }
    }

    if (capture && (IsKeyboardMessage(message) || IsMouseMessage(message))) {
        // Forward release events to the game: a key held before opening
        // the menu must never remain stuck after the menu is closed.
        if (message == WM_KEYUP || message == WM_SYSKEYUP ||
            message == WM_LBUTTONUP || message == WM_RBUTTONUP ||
            message == WM_MBUTTONUP || message == WM_XBUTTONUP)
            return DefSubclassProc(window, message, wparam, lparam);
        if (message == WM_SETCURSOR) {
            SetCursor(nullptr); // ImGui draws its own cursor while active.
            return TRUE;
        }
        return 0;
    }

    if (capture && message == WM_INPUT) {
        RawSample sample{};
        if (!DecodeRawInput(lparam, sample))
            return DefSubclassProc(window, message, wparam, lparam);

        // Copy raw data NOW, on the window thread. A HRAWINPUT handle may
        // become invalid before the DX12 Present thread processes it.
        {
            std::lock_guard lock(g_stateMutex);
            if (g_gate.MayCaptureInput() && g_rawSamples.size() < 1024)
                g_rawSamples.push_back(sample);
        }

        if (ForwardToGame(sample))
            return DefSubclassProc(window, message, wparam, lparam);
        // Normal RIM_INPUT housekeeping; do not forward camera movement and
        // key/button presses to Cyberpunk while m1core owns input.
        return DefWindowProcW(window, message, wparam, lparam);
    }

    return DefSubclassProc(window, message, wparam, lparam);
}

void AllocSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
              D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
{
    auto& graphics = *static_cast<Graphics*>(info->UserData);
    auto cpuBase = graphics.srvHeap->GetCPUDescriptorHandleForHeapStart();
    auto gpuBase = graphics.srvHeap->GetGPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kSrvDescriptors; ++i) {
        if (graphics.descriptors[i]) continue;
        graphics.descriptors[i] = true;
        cpu->ptr = cpuBase.ptr + static_cast<SIZE_T>(i) * graphics.descriptorStride;
        gpu->ptr = gpuBase.ptr + static_cast<UINT64>(i) * graphics.descriptorStride;
        return;
    }
    *cpu = {};
    *gpu = {};
}

void FreeSrv(ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu,
             D3D12_GPU_DESCRIPTOR_HANDLE)
{
    auto& graphics = *static_cast<Graphics*>(info->UserData);
    auto base = graphics.srvHeap->GetCPUDescriptorHandleForHeapStart().ptr;
    if (cpu.ptr < base || !graphics.descriptorStride) return;
    auto offset = cpu.ptr - base;
    if (offset % graphics.descriptorStride) return;
    auto index = offset / graphics.descriptorStride;
    if (index < kSrvDescriptors)
        graphics.descriptors[index] = false;
}

void WaitForGpu()
{
    auto& gfx = g_graphics;
    if (!gfx.queue || !gfx.fence || !gfx.fenceEvent) return;
    const UINT64 last = ++gfx.nextFence;
    if (FAILED(gfx.queue->Signal(gfx.fence.Get(), last))) return;
    if (gfx.fence->GetCompletedValue() < last) {
        if (SUCCEEDED(gfx.fence->SetEventOnCompletion(last, gfx.fenceEvent)))
            WaitForSingleObject(gfx.fenceEvent, 3000);
    }
}

void DestroyGraphics()
{
    {
        std::lock_guard lock(g_stateMutex);
        g_gate.SetRendererReady(false);
        g_messages.clear();
        g_rawSamples.clear();
    }
    SetCursorCaptured(false);
    auto& gfx = g_graphics;
    WaitForGpu();
    if (gfx.dx12Ready) ImGui_ImplDX12_Shutdown();
    if (gfx.win32Ready) ImGui_ImplWin32_Shutdown();
    if (gfx.contextCreated) ImGui::DestroyContext();
    gfx.dx12Ready = false;
    gfx.win32Ready = false;
    gfx.contextCreated = false;
    gfx.ready = false;
    gfx.frames.clear();
    gfx.commandList.Reset();
    gfx.rtvHeap.Reset();
    gfx.srvHeap.Reset();
    gfx.fence.Reset();
    gfx.queue.Reset();
    gfx.device.Reset();
    if (gfx.fenceEvent) {
        CloseHandle(gfx.fenceEvent);
        gfx.fenceEvent = nullptr;
    }
    gfx.nextFence = 0;
    gfx.descriptors.fill(false);
}

bool InitializeGraphics(IDXGISwapChain* swapchain)
{
    auto& gfx = g_graphics;
    auto* data = RED4ext::GpuApi::GetDeviceData();
    if (!data || !data->directCommandQueue || !g_window) return false;

    ComPtr<IDXGISwapChain3> chain;
    if (FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&chain)))) return false;

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapchain->GetDesc(&desc)) || !desc.BufferCount ||
        desc.BufferCount > kMaxBuffers || desc.OutputWindow != g_window)
        return false;

    if (FAILED(swapchain->GetDevice(IID_PPV_ARGS(&gfx.device)))) return false;
    gfx.queue = data->directCommandQueue;

    D3D12_DESCRIPTOR_HEAP_DESC heap{};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap.NumDescriptors = desc.BufferCount;
    if (FAILED(gfx.device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&gfx.rtvHeap))))
        return false;
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap.NumDescriptors = kSrvDescriptors;
    heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(gfx.device->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&gfx.srvHeap))))
        return false;

    gfx.descriptorStride =
        gfx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const UINT rtvStride = gfx.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto rtv = gfx.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    gfx.frames.resize(desc.BufferCount);
    for (UINT i = 0; i < desc.BufferCount; ++i) {
        Frame& frame = gfx.frames[i];
        if (FAILED(chain->GetBuffer(i, IID_PPV_ARGS(&frame.buffer)))) return false;
        frame.rtv = rtv;
        gfx.device->CreateRenderTargetView(frame.buffer.Get(), nullptr, rtv);
        rtv.ptr += rtvStride;
        if (FAILED(gfx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                        IID_PPV_ARGS(&frame.allocator))))
            return false;
    }
    if (FAILED(gfx.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            gfx.frames.front().allocator.Get(), nullptr, IID_PPV_ARGS(&gfx.commandList))))
        return false;
    if (FAILED(gfx.commandList->Close())) return false;
    if (FAILED(gfx.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gfx.fence))))
        return false;
    gfx.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!gfx.fenceEvent) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    gfx.contextCreated = true;
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().IniFilename = nullptr; // Never write imgui.ini into the game directory.
    ImGui::StyleColorsDark();
    gfx.win32Ready = ImGui_ImplWin32_Init(g_window);
    if (!gfx.win32Ready) return false;

    ImGui_ImplDX12_InitInfo info{};
    info.Device = gfx.device.Get();
    info.CommandQueue = gfx.queue.Get();
    info.NumFramesInFlight = static_cast<int>(desc.BufferCount);
    info.RTVFormat = desc.BufferDesc.Format;
    info.SrvDescriptorHeap = gfx.srvHeap.Get();
    info.UserData = &gfx;
    info.SrvDescriptorAllocFn = AllocSrv;
    info.SrvDescriptorFreeFn = FreeSrv;
    gfx.dx12Ready = ImGui_ImplDX12_Init(&info);
    if (!gfx.dx12Ready) return false;

    gfx.ready = true;
    {
        std::lock_guard lock(g_stateMutex);
        g_gate.SetRendererReady(true);
    }
    Log("m1core overlay: renderer ready; F11 toggle, ESC close, Ctrl+Shift+F11 emergency disable");
    return true;
}

void ProcessHotkeys()
{
    const bool f11 = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    const bool esc = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    const bool emergency = f11 && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                                    (GetAsyncKeyState(VK_SHIFT) & 0x8000);
    if (emergency && !g_emergencyWasDown)
        EmergencyDisable();
    g_emergencyWasDown = emergency;

    bool capture = false;
    {
        std::lock_guard lock(g_stateMutex);
        g_gate.SetGameFocused(IsFocused());
        if (g_enabled.load()) {
            if (f11 && !g_f11WasDown && !emergency)
                g_gate.Toggle();
            if (esc && !g_escapeWasDown)
                g_gate.Escape();
        }
        capture = g_gate.MayCaptureInput();
        if (!capture) {
            g_messages.clear();
            g_rawSamples.clear();
        }
    }
    g_f11WasDown = f11;
    g_escapeWasDown = esc;

    // Never call user32 trampolines while holding the state mutex. Both
    // open and close transitions must update the game's cursor clip.
    SetCursorCaptured(capture);

    if (capture != g_wasCapturingLastFrame) {
        g_rawBridge.Reset();
        g_wasCapturingLastFrame = capture;
    }
}

void DisableOverlayAfterRenderFailure(const char* reason)
{
    g_renderError.store(true);
    g_enabled.store(false); // Also stop intercepting F11 and legacy input.
    CloseOverlay();         // Always restore the game's cursor/clip.
    Warn(reason);
}

void Draw(IDXGISwapChain* swapchain)
{
    std::lock_guard renderLock(g_renderMutex);
    if (!g_enabled.load() || g_renderError.load()) return;

    if (!g_graphics.ready && !InitializeGraphics(swapchain)) {
        DestroyGraphics();
        DisableOverlayAfterRenderFailure("m1core overlay: renderer initialization failed; disabled safely");
        return;
    }
    ProcessHotkeys();
    if (!g_enabled.load()) return;

    // Do not discard mouse/key packets if this back buffer is still busy.
    // They remain queued until a frame can actually render.
    {
        std::lock_guard lock(g_stateMutex);
        if (!g_gate.MayCaptureInput()) return;
    }

    auto& gfx = g_graphics;
    ComPtr<IDXGISwapChain3> chain;
    if (FAILED(swapchain->QueryInterface(IID_PPV_ARGS(&chain)))) {
        DisableOverlayAfterRenderFailure("m1core overlay: swap chain query failed; disabling input capture");
        return;
    }
    const auto index = chain->GetCurrentBackBufferIndex();
    if (index >= gfx.frames.size()) {
        DisableOverlayAfterRenderFailure("m1core overlay: invalid back buffer index; disabling input capture");
        return;
    }
    auto& frame = gfx.frames[index];
    // Never reset a command allocator while the GPU is still using it.
    if (frame.pendingFence && gfx.fence->GetCompletedValue() < frame.pendingFence) {
        if (++g_pendingFrames > 120)
            DisableOverlayAfterRenderFailure("m1core overlay: graphics queue stalled; disabling input capture");
        return;
    }
    g_pendingFrames = 0;

    std::vector<MSG> messages;
    std::vector<RawSample> rawSamples;
    {
        std::lock_guard lock(g_stateMutex);
        if (!g_gate.MayCaptureInput()) return;
        messages.swap(g_messages);
        rawSamples.swap(g_rawSamples);
    }

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::GetIO().MouseDrawCursor = true;
    // The game may use RIDEV_NOLEGACY and recenter the physical pointer.
    // ImGui must instead consume the raw deltas, buttons, wheel and keys.
    g_rawBridge.Feed(g_window, messages, rawSamples);
    ImGui::NewFrame();
    // The background is a separate full-viewport ImGui layer; mod settings
    // remain freely movable windows above it, as in CET.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        ImVec2(0.0F, 0.0F), io.DisplaySize, IM_COL32(0, 0, 0, 140));
    bool windowOpen = true;
    if (g_registry) RenderSettings(*g_registry, windowOpen);
    if (!windowOpen)
        CloseOverlay();
    ImGui::Render();

    if (FAILED(frame.allocator->Reset())) {
        DisableOverlayAfterRenderFailure("m1core overlay: allocator reset failed; disabling input capture");
        return;
    }
    if (FAILED(gfx.commandList->Reset(frame.allocator.Get(), nullptr))) {
        DisableOverlayAfterRenderFailure("m1core overlay: command list reset failed; disabling input capture");
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = frame.buffer.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    gfx.commandList->ResourceBarrier(1, &barrier);
    gfx.commandList->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
    ID3D12DescriptorHeap* heaps[] = { gfx.srvHeap.Get() };
    gfx.commandList->SetDescriptorHeaps(1, heaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), gfx.commandList.Get());
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    gfx.commandList->ResourceBarrier(1, &barrier);
    if (FAILED(gfx.commandList->Close())) {
        DisableOverlayAfterRenderFailure("m1core overlay: command list close failed; disabling input capture");
        return;
    }
    ID3D12CommandList* lists[] = { gfx.commandList.Get() };
    gfx.queue->ExecuteCommandLists(1, lists);
    frame.pendingFence = ++gfx.nextFence;
    if (FAILED(gfx.queue->Signal(gfx.fence.Get(), frame.pendingFence)))
        DisableOverlayAfterRenderFailure("m1core overlay: command queue signal failed; disabling input capture");
}

HRESULT STDMETHODCALLTYPE PresentHook(IDXGISwapChain* chain, UINT sync, UINT flags)
{
    if (g_enabled.load() && chain == g_selectedChain && !(flags & DXGI_PRESENT_TEST))
        Draw(chain);
    return g_present(chain, sync, flags);
}

HRESULT STDMETHODCALLTYPE ResizeHook(IDXGISwapChain* chain, UINT count, UINT width,
                                     UINT height, DXGI_FORMAT format, UINT flags)
{
    if (chain == g_selectedChain) {
        std::lock_guard lock(g_renderMutex);
        DestroyGraphics(); // Release back buffer refs before DXGI resize.
    }
    return g_resize(chain, count, width, height, format, flags);
}

HRESULT STDMETHODCALLTYPE Resize1Hook(IDXGISwapChain* chain, UINT count, UINT width,
                                      UINT height, DXGI_FORMAT format, UINT flags,
                                      const UINT* nodes, IUnknown* const* queues)
{
    if (chain == g_selectedChain) {
        std::lock_guard lock(g_renderMutex);
        DestroyGraphics();
    }
    return g_resize1(chain, count, width, height, format, flags, nodes, queues);
}

bool OnRunningUpdate(RED4ext::CGameApplication*)
{
    if (!g_enabled.load() || g_hooked || !g_sdk || !g_sdk->hooking) return false;
    auto* data = RED4ext::GpuApi::GetDeviceData();
    if (!data || !data->directCommandQueue) return false;
    for (auto& handle : data->swapChains.resources) {
        if (!handle.IsUsed()) continue;
        auto& chainData = handle.instance;
        if (!chainData.swapChain || !chainData.windowHandle ||
            !IsWindow(chainData.windowHandle)) continue;
        // Win32 subclassing must run on the window-owning thread. If the
        // RED4ext Running callback uses another thread, do not install.
        if (GetCurrentThreadId() != GetWindowThreadProcessId(chainData.windowHandle, nullptr)) {
            if (!g_loggedThreadMismatch) {
                Warn("m1core overlay: RED4ext game callback is not the window thread; waiting (no input hook)");
                g_loggedThreadMismatch = true;
            }
            continue;
        }
        g_window = chainData.windowHandle;
        g_selectedChain = chainData.swapChain.Get();
        if (!SetWindowSubclass(g_window, WindowSubclass, kSubclassId, 0)) {
            Warn("m1core overlay: window subclass failed; no renderer hooks installed");
            g_enabled.store(false);
            return false;
        }
        g_subclassed = true;
        auto** vtable = *reinterpret_cast<void***>(g_selectedChain);
        g_presentTarget = vtable[8];
        g_resizeTarget = vtable[13];
        g_resize1Target = vtable[39];

        auto attach = g_sdk->hooking->Attach;
        const bool present = attach(g_handle, g_presentTarget, reinterpret_cast<void*>(&PresentHook),
                                    reinterpret_cast<void**>(&g_present));
        const bool resize = present && attach(g_handle, g_resizeTarget,
                reinterpret_cast<void*>(&ResizeHook), reinterpret_cast<void**>(&g_resize));
        const bool resize1 = resize && attach(g_handle, g_resize1Target,
                reinterpret_cast<void*>(&Resize1Hook), reinterpret_cast<void**>(&g_resize1));
        if (!resize1) {
            if (resize) g_sdk->hooking->Detach(g_handle, g_resizeTarget);
            if (present) g_sdk->hooking->Detach(g_handle, g_presentTarget);
            RemoveWindowSubclass(g_window, WindowSubclass, kSubclassId);
            g_subclassed = false;
            g_window = nullptr;
            g_selectedChain = nullptr;
            g_enabled.store(false);
            Warn("m1core overlay: one or more DXGI hooks failed; disabled safely");
            return false;
        }
        g_hooked = true;
        if (InstallCursorDetours(g_handle, g_sdk, g_window))
            Log("m1core overlay: cursor recenter/clip detours installed");
        else
            Warn("m1core overlay: cursor detours unavailable; using RAWINPUT virtual cursor fallback");
        Log("m1core overlay: DXGI hooks installed (development opt-in)");
        return false;
    }
    return false;
}

bool OnRunningExit(RED4ext::CGameApplication*)
{
    StopOverlay();
    return true;
}

RED4ext::v1::GameState g_runningState{
    .OnEnter = nullptr,
    .OnUpdate = &OnRunningUpdate,
    .OnExit = &OnRunningExit
};

} // namespace

void StartOverlay(Registry& registry, const std::filesystem::path& directory,
                  RED4ext::v1::PluginHandle handle, const RED4ext::v1::Sdk* sdk)
{
    g_registry = &registry;
    g_handle = handle;
    g_sdk = sdk;
    std::error_code error;
    const bool optedIn = std::filesystem::exists(directory / L"overlay.enable", error);
    if (error || !optedIn) {
        Log("m1core overlay: disabled (no overlay.enable marker)");
        return;
    }
    if (!sdk || !sdk->gameStates || !sdk->hooking ||
        !sdk->gameStates->Add(handle, RED4ext::EGameStateType::Running, &g_runningState)) {
        Warn("m1core overlay: unable to register game state; overlay stays disabled");
        return;
    }
    // A tiny local preview for testing the UI before external mods migrate.
    const auto mod = RED4ext::CNamePool::Add("m1core.preview");
    registry.RegisterBool(mod.hash, RED4ext::CNamePool::Add("Enabled").hash, true);
    registry.RegisterInt(mod.hash, RED4ext::CNamePool::Add("Level").hash, 5, 0, 20);
    registry.RegisterFloat(mod.hash, RED4ext::CNamePool::Add("Multiplier").hash,
                           1.0F, 0.5F, 5.0F);
    g_enabled.store(true);
    Log("m1core overlay: opted in; waiting for Cyberpunk swap chain");
}

void StopOverlay()
{
    g_enabled.store(false);
    CloseOverlay();
    RemoveCursorDetours();
    if (g_hooked && g_sdk && g_sdk->hooking) {
        g_sdk->hooking->Detach(g_handle, g_resize1Target);
        g_sdk->hooking->Detach(g_handle, g_resizeTarget);
        g_sdk->hooking->Detach(g_handle, g_presentTarget);
        g_hooked = false;
    }
    {
        std::lock_guard lock(g_renderMutex);
        DestroyGraphics();
    }
    if (g_subclassed && g_window &&
        GetCurrentThreadId() == GetWindowThreadProcessId(g_window, nullptr)) {
        RemoveWindowSubclass(g_window, WindowSubclass, kSubclassId);
        g_subclassed = false;
    }
    g_selectedChain = nullptr;
    g_registry = nullptr;
}

} // namespace m1core
