cmake_minimum_required(VERSION 3.23)
project(m1core VERSION 0.1.0 LANGUAGES CXX)

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
)
target_include_directories(m1core_imgui PUBLIC "${dear_imgui_SOURCE_DIR}")

add_library(m1core_settings STATIC src/settings.cpp)
target_include_directories(m1core_settings PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(m1core_settings PUBLIC nlohmann_json::nlohmann_json)
if(WIN32)
    target_compile_definitions(m1core_settings PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(WIN32)
    add_library(m1core SHARED src/plugin.cpp src/ui.cpp)
    target_link_libraries(m1core PRIVATE m1core_settings m1core_imgui RED4ext::SDK)
    target_compile_definitions(m1core PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE RED4EXT_HEADER_ONLY)
    set_target_properties(m1core PROPERTIES OUTPUT_NAME "m1core" PREFIX "")
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(m1core_settings_tests tests/settings_tests.cpp)
    target_link_libraries(m1core_settings_tests PRIVATE m1core_settings)
    add_test(NAME m1core_settings_tests COMMAND m1core_settings_tests)
endif()
