set_xmakever("3.1")

set_languages("cxx20")
set_arch("x64")

add_rules("mode.debug","mode.releasedbg", "mode.release")
add_rules("c.unity_build")

add_cxflags("/bigobj", "/MP", "/EHsc")
add_defines("UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS")

if is_mode("debug") then
    set_optimize("none")
    set_warnings("all")
    set_policy("build.optimization.lto", false)
elseif is_mode("releasedbg") then
    set_optimize("fastest")
    set_warnings("all")
    set_policy("build.optimization.lto", true)
elseif is_mode("release") then
    set_strip("all")
    set_optimize("fastest")
    set_warnings("all")
    set_policy("build.optimization.lto", true)
end

set_symbols("debug")
set_runtimes(is_mode("release") and "MD" or "MDd");

add_requireconfs("**", { configs = {
    --debug = is_mode("debug"), -- This seems to cause compilation issues recently and probably has little benefit anyway
    lto = not is_mode("debug"),
    shared = false,
    vs_runtime = is_mode("release") and "MD" or "MDd" } })

add_requires("spdlog 1.11.0")
add_requires("nlohmann_json")
add_requires("hopscotch-map")
add_requires("minhook")
add_requires("mem")
add_requires("tiltedcore 0.2.9")
add_requires("sqlite3")
add_requires("xbyak")
add_requires("stb")
add_requires("sol2", { configs = { includes_lua = false } })
add_requires("openrestry-luajit", { configs = { gc64 = true } })
add_requireconfs("tiltedcore.mimalloc", {version = "2.2.4", override = true, configs = {rltgenrandom = true}})

local imguiUserConfig = string.gsub(path.absolute("src/imgui_impl/imgui_user_config.h"), "\\", "/")
add_requires("imgui v1.91.1-docking", { configs = { wchar32 = true, freetype = true, user_config = imguiUserConfig } })


-- Keep the original CET 1.38.1 dependency graph: its renderer, keybindings,
-- Win32 backend, Lua runtime and game hooks are the foundation, not wrappers
-- around the previous independent m1core renderer.

target("RED4ext.SDK")
    set_kind("headeronly")
    set_group("vendor")
    add_headerfiles("vendor/RED4ext.SDK/include/**.hpp")
    add_includedirs("vendor/RED4ext.SDK/include", {public = true})
    on_install(function() end)

target("m1core")
    set_kind("shared")
    set_filename("m1core.dll")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX", "WINVER=0x0601",
        "SOL_ALL_SAFETIES_ON", "SOL_LUAJIT=1", "SOL_EXCEPTIONS_SAFE_PROPAGATION",
        "SPDLOG_WCHAR_TO_UTF8_SUPPORT", "SPDLOG_WCHAR_FILENAMES",
        "SPDLOG_WCHAR_SUPPORT",
        "IMGUI_USER_CONFIG=\"" .. imguiUserConfig .. "\"")
    set_pcxxheader("src/stdafx.h")

    set_configdir(path.join("src", "config"))
    add_configfiles("src/xmake/M1CoreVersion.h.in")
    -- RED4ext invokes Main/Query/Supports; the old ASI DllMain is removed.
    add_files("src/**.cpp", "../native/state.cpp", "../native/registry.cpp")
    add_headerfiles("src/**.h", "src/**.hpp")
    add_includedirs("src", "build", "../native")
    add_syslinks("User32", "Version", "d3d11", "d3d12", "d3dcompiler",
                 "dxgi", "imm32", "Dbghelp")
    add_packages("spdlog", "nlohmann_json", "minhook", "hopscotch-map",
        "imgui", "mem", "sol2", "tiltedcore", "sqlite3",
        "openrestry-luajit", "xbyak", "stb")
    add_deps("RED4ext.SDK")

    set_configvar("M1CORE_PRODUCT_NAME", "m1core")
    on_load(function(target)
        local cfg = target:get("configvar")
        cfg.M1CORE_CURRENT_YEAR = tonumber(os.date("%Y"))
        cfg.M1CORE_PRODUCT_NAME = "m1core"
        cfg.M1CORE_GROUP_NAME = "MISO"
        cfg.M1CORE_FILE_NAME = "m1core.dll"
        cfg.M1CORE_DEBUG_BUILD = is_mode("release") and 0 or 1
        cfg.M1CORE_GIT_TAG = "v1.0.0"
        cfg.M1CORE_GIT_TAG_LONG = "v1.0.0-0-g0000000"
        cfg.M1CORE_GIT_BRANCH = "dev/m1core-cet-foundation"
        cfg.M1CORE_GIT_COMMIT = "0000000"
        cfg.M1CORE_GIT_COMMIT_LONG = "0000000000000000000000000000000000000000"
        cfg.M1CORE_GIT_COMMIT_DATE = "20261121"
        cfg.M1CORE_VERSION_MAJOR = 1
        cfg.M1CORE_VERSION_MINOR = 0
        cfg.M1CORE_VERSION_PATCH = 0
        cfg.M1CORE_VERSION_SIMPLE = "1.0.0"
        cfg.M1CORE_VERSION_FULL = "1.0.0-dev"
        cfg.M1CORE_VERSION = cfg.M1CORE_VERSION_FULL
        target:add("configvar",cfg)
        if not is_mode("release") then target:add("defines","M1CORE_DEBUG") end
    end)

-- Native storage/registry tests run against the SAME source files compiled
-- into the CET-derived core, without launching Cyberpunk or its input hooks.
target("m1core_registry_tests")
    set_kind("binary")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_files("../native/state.cpp", "../native/registry.cpp", "../tests/registry_tests.cpp")
    add_includedirs("../native", "../tests")
    add_packages("nlohmann_json")

target("m1core_state_tests")
    set_kind("binary")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_files("../native/state.cpp", "../tests/state_tests.cpp")
    add_includedirs("../native", "../tests")
    add_packages("nlohmann_json")

target("m1core_script_formats_tests")
    set_kind("binary")
    add_files("../tests/script_formats_tests.cpp")
    add_includedirs("src", "../tests")


-- Execute real LuaJIT source and generated bytecode, not just extension names.
target("m1core_script_runtime_tests")
    set_kind("binary")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_files("../tests/script_runtime_tests.cpp")
    add_includedirs("src", "../tests")
    add_packages("sol2", "openrestry-luajit")
