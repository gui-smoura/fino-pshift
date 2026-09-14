cmake_minimum_required(VERSION 3.20)
project(fino-pshift VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Configure warning flags as errors according to GEMINI.md
if(MSVC)
    set(FINO_WARNING_FLAGS /W4 /WX /permissive- /utf-8)
else()
    set(FINO_WARNING_FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

include(FetchContent)

# 1. Signalsmith Stretch (Pitch Shifter DSP)
message(STATUS "Fetching Signalsmith Stretch...")
FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/signalsmith-audio/signalsmith-stretch.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)

# 2. Miniaudio (Audio I/O)
message(STATUS "Fetching miniaudio...")
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

# 3. GLFW (Windowing & OpenGL Context)
message(STATUS "Fetching GLFW...")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

# 4. Dear ImGui (GUI)
message(STATUS "Fetching Dear ImGui...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5
    GIT_SHALLOW TRUE
)

# 5. nlohmann/json (Settings & Presets Persistence)
message(STATUS "Fetching nlohmann_json...")
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

# 6. Catch2 (Unit Tests)
message(STATUS "Fetching Catch2...")
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(signalsmith_stretch miniaudio glfw imgui nlohmann_json Catch2)

# Create library for Dear ImGui with GLFW + OpenGL3 backends
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib SYSTEM PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC glfw)
if(WIN32)
    target_link_libraries(imgui_lib PUBLIC opengl32)
else()
    target_link_libraries(imgui_lib PUBLIC GL)
endif()

# Core DSP & Audio Engine Library (fino_core)
add_library(fino_core STATIC
    src/pitch_processor.cpp
    src/audio_engine.cpp
    src/config_manager.cpp
)

target_include_directories(fino_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(fino_core SYSTEM PUBLIC
    ${signalsmith_stretch_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}
)

target_link_libraries(fino_core PUBLIC
    nlohmann_json::nlohmann_json
    signalsmith-stretch
)

target_compile_options(fino_core PRIVATE ${FINO_WARNING_FLAGS})

# UI & Main Executable (fino-pshift)
add_executable(fino-pshift
    src/ui.cpp
    src/main.cpp
)

target_link_libraries(fino-pshift PRIVATE
    fino_core
    imgui_lib
    glfw
)

if(WIN32)
    set_target_properties(fino-pshift PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        set_target_properties(fino-pshift PROPERTIES LINK_FLAGS "/ENTRY:mainCRTStartup")
    endif()
endif()

target_compile_options(fino-pshift PRIVATE ${FINO_WARNING_FLAGS})

# Unit Tests
enable_testing()
add_subdirectory(tests)
