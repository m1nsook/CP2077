cmake_minimum_required(VERSION 3.23)
project(m1core VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# The pinned SDK revision includes the Cyberpunk 2077 2.50 runtime.
# Do not replace it with the SDK's older 1.0.0 release tag.
set(RED4EXT_HEADER_ONLY ON CACHE BOOL "" FORCE)
set(RED4EXT_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    red4ext_sdk
    GIT_REPOSITORY https://github.com/wopss/RED4ext.SDK.git
    GIT_TAG a34f0ad0a9e4f37cbe1fe001e4ff68c2949d0665
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    dear_imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG 9a5d5c45f54b1301ea471622eddede70384243af
)
FetchContent_MakeAvailable(red4ext_sdk nlohmann_json dear_imgui)

add_library(m1core_imgui STATIC
    ${dear_imgui_SOURCE_DIR}/imgui.cpp
    ${dear_imgui_SOURCE_DIR}/imgui_draw.cpp
    ${dear_imgui_SOURCE_DIR}/imgui_tables.cpp
    ${dear_imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${dear_imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${dear_imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(m1core_imgui PUBLIC "${dear_imgui_SOURCE_DIR}")
if(WIN32)
    target_link_libraries(m1core_imgui PUBLIC d3d12 dxgi d3dcompiler imm32)
    target_compile_definitions(m1core_imgui PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

add_library(m1core_diagnostics STATIC src/diagnostics.cpp)
target_include_directories(m1core_diagnostics PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(m1core_keybinds STATIC src/keybind_actions.cpp)
target_include_directories(m1core_keybinds PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(m1core_settings STATIC src/settings.cpp)
target_include_directories(m1core_settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(m1core_settings PUBLIC nlohmann_json::nlohmann_json)
if(WIN32)
    target_compile_definitions(m1core_settings PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(WIN32)
    add_library(m1core SHARED
        src/plugin.cpp
        src/ui.cpp
        src/ui_components.cpp
        src/ui_theme.cpp
        src/overlay.cpp
        src/cursor.cpp
        src/raw_input.cpp
    )
    target_link_libraries(m1core PRIVATE m1core_settings m1core_diagnostics m1core_keybinds m1core_imgui RED4ext::SDK comctl32)
    target_compile_definitions(m1core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE RED4EXT_HEADER_ONLY)
    set_target_properties(m1core PROPERTIES OUTPUT_NAME "m1core" PREFIX "")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(m1core_settings_tests tests/settings_tests.cpp)
    target_link_libraries(m1core_settings_tests PRIVATE m1core_settings)
    add_test(NAME m1core_settings_tests COMMAND m1core_settings_tests)

    add_executable(m1core_startup_gate_tests tests/startup_gate_tests.cpp)
    target_include_directories(m1core_startup_gate_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME m1core_startup_gate_tests COMMAND m1core_startup_gate_tests)

    add_executable(m1core_keybind_actions_tests tests/keybind_actions_tests.cpp)
    target_link_libraries(m1core_keybind_actions_tests PRIVATE m1core_keybinds)
    add_test(NAME m1core_keybind_actions_tests COMMAND m1core_keybind_actions_tests)

    add_executable(m1core_diagnostics_tests tests/diagnostics_tests.cpp)
    target_link_libraries(m1core_diagnostics_tests PRIVATE m1core_diagnostics)
    add_test(NAME m1core_diagnostics_tests COMMAND m1core_diagnostics_tests)

    add_executable(m1core_input_gate_tests tests/input_gate_tests.cpp)
    target_include_directories(m1core_input_gate_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME m1core_input_gate_tests COMMAND m1core_input_gate_tests)

    add_executable(m1core_pointer_transform_tests tests/pointer_transform_tests.cpp)
    target_include_directories(m1core_pointer_transform_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME m1core_pointer_transform_tests COMMAND m1core_pointer_transform_tests)

    add_executable(m1core_workspace_layout_tests tests/workspace_layout_tests.cpp)
    target_include_directories(m1core_workspace_layout_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME m1core_workspace_layout_tests COMMAND m1core_workspace_layout_tests)

    add_executable(m1core_ui_components_tests tests/ui_components_tests.cpp)
    target_include_directories(m1core_ui_components_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME m1core_ui_components_tests COMMAND m1core_ui_components_tests)

    add_executable(m1core_keybind_conflicts_tests tests/keybind_conflicts_tests.cpp)
    target_link_libraries(m1core_keybind_conflicts_tests PRIVATE m1core_settings)
    add_test(NAME m1core_keybind_conflicts_tests COMMAND m1core_keybind_conflicts_tests)

    if(WIN32)
        add_executable(m1core_menu_shortcut_tests tests/menu_shortcut_tests.cpp)
        target_include_directories(m1core_menu_shortcut_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_definitions(m1core_menu_shortcut_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        add_test(NAME m1core_menu_shortcut_tests COMMAND m1core_menu_shortcut_tests)

        add_executable(m1core_raw_input_tests tests/raw_input_tests.cpp src/raw_input.cpp)
        target_include_directories(m1core_raw_input_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(m1core_raw_input_tests PRIVATE m1core_imgui)
        target_compile_definitions(m1core_raw_input_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        add_test(NAME m1core_raw_input_tests COMMAND m1core_raw_input_tests)
    endif()
endif()
