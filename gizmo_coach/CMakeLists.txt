cmake_minimum_required(VERSION 3.16)
project(gizmo_coach LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# -----------------------------------------------------------------------------
# JSON (nlohmann) via FetchContent
# -----------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# -----------------------------------------------------------------------------
# HTTP server library (cpp-httplib) - header only
# -----------------------------------------------------------------------------
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG v0.15.3
)
FetchContent_MakeAvailable(httplib)

# -----------------------------------------------------------------------------
# Dein Hauptprogramm
# -----------------------------------------------------------------------------
add_executable(gizmo_coach
    src/main.cpp
    src/brain_io.cpp
    src/coach_logic.cpp
    src/hormons_reader.cpp
    src/livekit_stub.cpp
    src/pattern_gen.cpp
)

# -----------------------------------------------------------------------------
# Includes
# -----------------------------------------------------------------------------
target_include_directories(gizmo_coach PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${httplib_SOURCE_DIR}
    
)

# -----------------------------------------------------------------------------
# Libraries
# -----------------------------------------------------------------------------
target_link_libraries(gizmo_coach PRIVATE
    nlohmann_json::nlohmann_json
)

# -----------------------------------------------------------------------------
# Platform-specific settings
# -----------------------------------------------------------------------------
if(WIN32)
    # Windows: keine zusätzlichen Flags nötig
    target_compile_definitions(gizmo_coach PRIVATE PLATFORM_WINDOWS)
else()
    # Linux/Raspberry Pi: Threads + Filesystem
    find_package(Threads REQUIRED)
    target_link_libraries(gizmo_coach PRIVATE Threads::Threads)
    target_compile_definitions(gizmo_coach PRIVATE PLATFORM_LINUX)
endif()