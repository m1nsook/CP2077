#include <stdafx.h>
#include "ImGuiCustomTheme.h"

#include "D3D12.h"
#include <overlay/FontPalette.h>
#include "Options.h"
#include "Utils.h"

#include <M1CoreRuntime.h>
#include <imgui_impl/dx12.h>
#include <imgui_impl/win32.h>
#include <window/window.h>

bool D3D12::ResetState(const bool acDestroyContext)
{
    if (m_initialized)
    {
        std::lock_guard _(m_imguiLock);

        for (auto& drawData : m_imguiDrawDataBuffers)
        {
            for (auto i = 0; i < drawData.CmdListsCount; ++i)
                IM_DELETE(drawData.CmdLists[i]);
            drawData.Clear();
        }

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();

        if (acDestroyContext)
            ImGui::DestroyContext();
    }

    m_frameContexts.clear();
    m_outSize = {0, 0};

    m_pd3d12Device.Reset();
    m_pd3dRtvDescHeap.Reset();
    m_pd3dSrvDescHeap.Reset();

    m_pCommandQueue.Reset();
    m_pdxgiSwapChain.Reset();

    m_initialized = false;

    return false;
}

bool D3D12::Initialize()
{
    if (m_initialized)
        return true;

    if (!m_pdxgiSwapChain)
        return false;

    const HWND hWnd = m_window.GetWindow();
    if (!hWnd)
    {
        Log::Warn("D3D12::Initialize() - window not yet hooked!");
        return false;
    }

    if (FAILED(m_pdxgiSwapChain->GetDevice(IID_PPV_ARGS(&m_pd3d12Device))))
    {
        Log::Error("D3D12::Initialize() - failed to get device!");
        return ResetState();
    }

    DXGI_SWAP_CHAIN_DESC sdesc;
    m_pdxgiSwapChain->GetDesc(&sdesc);

    if (hWnd != sdesc.OutputWindow)
        Log::Warn(
            "D3D12::Initialize() - output window of current swap chain does not match hooked window! Currently hooked "
            "to {} while swap chain output window is {}.",
            reinterpret_cast<void*>(hWnd), reinterpret_cast<void*>(sdesc.OutputWindow));

    m_outSize = {static_cast<LONG>(sdesc.BufferDesc.Width), static_cast<LONG>(sdesc.BufferDesc.Height)};

    const auto buffersCounts = std::min(sdesc.BufferCount, 3u);
    m_frameContexts.resize(buffersCounts);

    D3D12_DESCRIPTOR_HEAP_DESC srvdesc = {};
    srvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvdesc.NumDescriptors = 200; // Same number as is used in scripting/Texture
    srvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_pd3d12Device->CreateDescriptorHeap(&srvdesc, IID_PPV_ARGS(&m_pd3dSrvDescHeap))))
    {
        Log::Error("D3D12::Initialize() - failed to create SRV descriptor heap!");
        return ResetState();
    }

    D3D12_DESCRIPTOR_HEAP_DESC rtvdesc;
    rtvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvdesc.NumDescriptors = buffersCounts;
    rtvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvdesc.NodeMask = 1;
    if (FAILED(m_pd3d12Device->CreateDescriptorHeap(&rtvdesc, IID_PPV_ARGS(&m_pd3dRtvDescHeap))))
    {
        Log::Error("D3D12::Initialize() - failed to create RTV descriptor heap!");
        return ResetState();
    }

    const SIZE_T rtvDescriptorSize = m_pd3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < buffersCounts; i++)
    {
        auto& context = m_frameContexts[i];
        context.MainRenderTargetDescriptor = rtvHandle;
        m_pdxgiSwapChain->GetBuffer(i, IID_PPV_ARGS(&context.BackBuffer));
        m_pd3d12Device->CreateRenderTargetView(context.BackBuffer.Get(), nullptr, context.MainRenderTargetDescriptor);
        rtvHandle.ptr += rtvDescriptorSize;
    }

    for (auto& context : m_frameContexts)
    {
        if (FAILED(m_pd3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&context.CommandAllocator))))
        {
            Log::Error("D3D12::Initialize() - failed to create command allocator!");
            return ResetState();
        }

        if (FAILED(m_pd3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, context.CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&context.CommandList))) ||
            FAILED(context.CommandList->Close()))
        {
            Log::Error("D3D12::Initialize() - failed to create command list!");
            return ResetState();
        }
    }

    if (!InitializeImGui(buffersCounts))
    {
        Log::Error("D3D12::Initialize() - failed to initialize ImGui!");
        return ResetState();
    }

    Log::Info("D3D12::Initialize() - initialization successful!");
    m_initialized = true;

    OnInitialized.Emit();

    return true;
}

void D3D12::ReloadFonts()
{
    std::lock_guard _(m_imguiLock);
    auto& io = ImGui::GetIO();

    // Font references belong to this atlas; never retain dangling pointers.
    io.FontDefault = nullptr;
    m1core::SetLogFont(nullptr);
    m1core::SetKarlaWeights(nullptr, nullptr, nullptr);
    m1core::SetIconsAvailable(false);
    io.Fonts->Clear();

    // Karla/JetBrains Mono are independently licensed SIL OFL fonts,
    // bundled by CI with their own license notices. No donated CET fonts.
    static const ImWchar latinExtended[] = {
        0x0020, 0x024F, 0x2000, 0x206F, 0x20AC, 0x20AC, 0
    };
    const auto load = [&](const wchar_t* name, float size) -> ImFont*
    {
        const auto path = m_paths.Fonts() / name;
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
        {
            Log::Warn("m1core UI font is missing: {}", UTF16ToUTF8(path.native()));
            return nullptr;
        }

        ImFontConfig config{};
        config.OversampleH = m_options.Font.OversampleHorizontal;
        config.OversampleV = m_options.Font.OversampleVertical;
        // ImGui's Win32 font loader accepts paths in UTF-8.
        return io.Fonts->AddFontFromFileTTF(
            UTF16ToUTF8(path.native()).c_str(), size, &config, latinExtended);
    };

    const float size = std::clamp(m_options.Font.BaseSize, 12.0f, 30.0f);
    ImFont* mainFont = load(L"Karla-Regular.ttf", size);
    if (!mainFont)
    {
        // A damaged or old install must still show an operable settings UI.
        mainFont = io.Fonts->AddFontDefault();
        Log::Warn("Using Dear ImGui fallback font because Karla was unavailable");
    }
    io.FontDefault = mainFont;

    // Optional text-only Karla weights. Missing files fall back gracefully
    // to Regular for older installs. Their pointers are atlas-owned and are
    // reset above before any font reload.
    m1core::SetKarlaWeights(
        load(L"Karla-Medium.ttf", size),
        load(L"Karla-SemiBold.ttf", size),
        load(L"Karla-Bold.ttf", size));

    // Merge only the needed Phosphor regular icons into Karla, keeping
    // fonts in a single normal ImGui atlas and without custom DrawList UI.
    // Codepoints come from phosphor-icons/web src/regular/style.css.
    static const ImWchar phosphorRanges[] = {
        0xE094, 0xE094, // arrows-clockwise
        0xE23A, 0xE23A, // file-text
        0xE248, 0xE248, // floppy-disk
        0xE2CE, 0xE2CE, // info
        0xE2D8, 0xE2D8, // keyboard
        0xE30C, 0xE30C, // magnifying-glass
        0xE3AE, 0xE3AE, // pencil
        0xE432, 0xE432, // sliders
        0xE4A6, 0xE4A6, // trash
        0xE4C4, 0xE4C4, // user-circle
        0xE4E0, 0xE4E0, // warning (triangle/exclamation)
        0xE4F6, 0xE4F6, // x (close)
        0
    };
    const auto iconPath = m_paths.Fonts() / L"Phosphor-Regular.ttf";
    std::error_code iconError;
    if (std::filesystem::is_regular_file(iconPath, iconError))
    {
        ImFontConfig iconConfig{};
        iconConfig.MergeMode = true;
        iconConfig.GlyphMinAdvanceX = size * 0.86f;
        // Phosphor's glyph outlines sit above Karla's text baseline.
        // Correct the alignment once for every merged icon (including
        // all five tab labels), instead of drawing/offsetting each tab.
        // Positive ImGui GlyphOffset.y moves only these icons down.
        iconConfig.GlyphOffset = ImVec2(0.0f, size * 0.125f);
        iconConfig.OversampleH = 2;
        iconConfig.OversampleV = 1;
        if (io.Fonts->AddFontFromFileTTF(
                UTF16ToUTF8(iconPath.native()).c_str(), size, &iconConfig, phosphorRanges))
            m1core::SetIconsAvailable(true);
        else
            Log::Warn("Failed to load Phosphor icons: using plain-text tab labels");
    }
    else
    {
        Log::Warn("Phosphor icon font not found: using plain-text tab labels");
    }

    ImFont* mono = load(L"JetBrainsMono-Regular.ttf", std::max(12.0f, size * 0.84f));
    if (mono)
        m1core::SetLogFont(mono);
    else
        Log::Warn("Using main UI font for logs because JetBrains Mono was unavailable");
}

bool D3D12::InitializeImGui(size_t aBuffersCounts)
{
    std::lock_guard _(m_imguiLock);

    // TODO - scale also by DPI
    const auto [resx, resy] = m_outSize;
    const auto scaleFromReference = std::min(static_cast<float>(resx) / 1920.0f, static_cast<float>(resy) / 1080.0f);

    if (ImGui::GetCurrentContext() == nullptr)
    {
        // do this once, do not repeat context creation!
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        // User-provided standard Dear ImGui theme, no custom widgets.
        ImGui::StyleColorsDark(&m_styleReference);
        m1CoreTheme(m_styleReference);
    }

    ImGui::GetStyle() = m_styleReference;
    ImGui::GetStyle().ScaleAllSizes(scaleFromReference);

    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    if (!ImGui_ImplWin32_Init(m_window.GetWindow()))
    {
        Log::Error("D3D12::InitializeImGui() - ImGui_ImplWin32_Init call failed!");
        return false;
    }

    if (!ImGui_ImplDX12_Init(
            m_pd3d12Device.Get(), static_cast<int>(aBuffersCounts), DXGI_FORMAT_R8G8B8A8_UNORM, m_pd3dSrvDescHeap.Get(), m_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
            m_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart()))
    {
        Log::Error("D3D12::InitializeImGui() - ImGui_ImplDX12_Init call failed!");
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    ReloadFonts();

    if (!ImGui_ImplDX12_CreateDeviceObjects(m_pCommandQueue.Get()))
    {
        Log::Error("D3D12::InitializeImGui() - ImGui_ImplDX12_CreateDeviceObjects call failed!");
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    return true;
}

void D3D12::PrepareUpdate()
{
    if (!m_initialized)
        return;

    std::lock_guard _(m_imguiLock);

    ImGui_ImplWin32_NewFrame(m_outSize);
    ImGui::NewFrame();

    M1CoreRuntime::Get().GetOverlay().Update();

    M1CoreRuntime::Get().GetVM().Draw();

    ImGui::Render();

    auto& drawData = m_imguiDrawDataBuffers[2];

    for (auto i = 0; i < drawData.CmdListsCount; ++i)
        IM_DELETE(drawData.CmdLists[i]);
    drawData.Clear();

    drawData = *ImGui::GetDrawData();

    ImVector<ImDrawList*> copiedDrawLists;
    copiedDrawLists.resize(drawData.CmdListsCount);

    for (auto i = 0; i < drawData.CmdListsCount; ++i)
        copiedDrawLists[i] = drawData.CmdLists[i]->CloneOutput();
    drawData.CmdLists = std::move(copiedDrawLists);

    std::swap(m_imguiDrawDataBuffers[1], m_imguiDrawDataBuffers[2]);
}

void D3D12::Update()
{
    // swap staging ImGui buffer with render ImGui buffer
    {
        std::lock_guard _(m_imguiLock);
        ImGui_ImplDX12_NewFrame(m_pCommandQueue.Get());
        if (m_imguiDrawDataBuffers[1].Valid)
        {
            std::swap(m_imguiDrawDataBuffers[0], m_imguiDrawDataBuffers[1]);
            m_imguiDrawDataBuffers[1].Valid = false;
        }
    }

    if (!m_imguiDrawDataBuffers[0].Valid)
        return;

    assert(m_pdxgiSwapChain);
    auto& frameContext = m_frameContexts[m_pdxgiSwapChain->GetCurrentBackBufferIndex()];
    frameContext.CommandAllocator->Reset();

    D3D12_RESOURCE_BARRIER barrier;
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = frameContext.BackBuffer.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    ID3D12DescriptorHeap* heaps[] = {m_pd3dSrvDescHeap.Get()};

    frameContext.CommandList->Reset(frameContext.CommandAllocator.Get(), nullptr);
    frameContext.CommandList->ResourceBarrier(1, &barrier);
    frameContext.CommandList->SetDescriptorHeaps(1, heaps);
    frameContext.CommandList->OMSetRenderTargets(1, &frameContext.MainRenderTargetDescriptor, FALSE, nullptr);

    ImGui_ImplDX12_RenderDrawData(&m_imguiDrawDataBuffers[0], frameContext.CommandList.Get());

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    frameContext.CommandList->ResourceBarrier(1, &barrier);
    frameContext.CommandList->Close();

    ID3D12CommandList* commandLists[] = {frameContext.CommandList.Get()};
    m_pCommandQueue->ExecuteCommandLists(1, commandLists);
}
