cmake_minimum_required(VERSION 3.23)
project(m1core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
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
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(red4ext_sdk nlohmann_json imgui)

add_library(m1core_imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp"
)
target_include_directories(m1core_imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_compile_definitions(m1core_imgui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(m1core_imgui PUBLIC d3d12 dxgi imm32 user32)

add_library(m1core_state STATIC native/state.cpp)
target_include_directories(m1core_state PUBLIC native)
target_link_libraries(m1core_state PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(m1core_state PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

add_library(m1core_registry STATIC native/registry.cpp)
target_include_directories(m1core_registry PUBLIC native)
target_link_libraries(m1core_registry PUBLIC m1core_state)

if(WIN32)
    add_library(m1core SHARED
        native/plugin.cpp
        native/render_bridge.cpp
        native/renderer.cpp
        native/input.cpp
        native/ui.cpp
    )
    target_link_libraries(m1core PRIVATE
        m1core_registry RED4ext::SDK m1core_imgui d3d12 dxgi user32
    )
    target_compile_definitions(m1core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE RED4EXT_HEADER_ONLY)
    set_target_properties(m1core PROPERTIES OUTPUT_NAME "m1core" PREFIX "")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(m1core_state_tests tests/state_tests.cpp)
    target_link_libraries(m1core_state_tests PRIVATE m1core_state)
    add_test(NAME m1core_state_tests COMMAND m1core_state_tests)

    add_executable(m1core_registry_tests tests/registry_tests.cpp)
    target_link_libraries(m1core_registry_tests PRIVATE m1core_registry)
    add_test(NAME m1core_registry_tests COMMAND m1core_registry_tests)

    add_executable(m1core_input_policy_tests tests/input_policy_tests.cpp)
    target_include_directories(m1core_input_policy_tests PRIVATE native)
    add_test(NAME m1core_input_policy_tests COMMAND m1core_input_policy_tests)
endif()
