#include <stdafx.h>

#include "M1CoreRuntime.h"
#include "Options.h"
#include "state.hpp"

using namespace std::chrono_literals;

static std::unique_ptr<M1CoreRuntime> s_pInstance{nullptr};
static bool s_isRunning{true};

void M1CoreRuntime::Initialize()
{
    s_pInstance.reset(new M1CoreRuntime);

    // Configure persistent settings before engine/Lua initialization can
    // register any mods. A late Configure would discard the values that
    // a mod saw during registration and make startup look like a reset.
    m1core::Store().Configure(s_pInstance->GetPaths().CoreRoot() / L"settings.json");

    s_pInstance->GetVM().Prepare();
}

void M1CoreRuntime::Shutdown()
{
    s_pInstance.reset(nullptr);
}

M1CoreRuntime& M1CoreRuntime::Get()
{
    // we should always call this after initialization, never before!
    assert(s_pInstance);
    return *s_pInstance;
}

const Paths& M1CoreRuntime::GetPaths() const noexcept
{
    return m_paths;
}

const Options& M1CoreRuntime::GetOptions() const noexcept
{
    return m_options;
}

const PersistentState& M1CoreRuntime::GetPersistentState() const noexcept
{
    return m_persistentState;
}

D3D12& M1CoreRuntime::GetD3D12() noexcept
{
    return m_d3d12;
}

VKBindings& M1CoreRuntime::GetBindings() noexcept
{
    return m_bindings;
}

Overlay& M1CoreRuntime::GetOverlay() noexcept
{
    return m_overlay;
}

LuaVM& M1CoreRuntime::GetVM() noexcept
{
    return m_vm;
}

bool M1CoreRuntime::IsRunning() noexcept
{
    return s_isRunning;
}

M1CoreRuntime::M1CoreRuntime()
    : m_options(m_paths)
    , m_persistentState(m_paths, m_options)
    , m_bindings(m_paths, m_options)
    , m_window(&m_bindings, &m_d3d12)
    , m_d3d12(m_window, m_paths, m_options)
    , m_vm(m_paths, m_options, m_bindings, m_d3d12)
    , m_overlay(m_bindings, m_options, m_persistentState, m_vm)
{
}

M1CoreRuntime::~M1CoreRuntime()
{
    s_isRunning = false;
}
