cmake_minimum_required(VERSION 3.20)
project(lyrics VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Options ──────────────────────────────────────────────────────
option(BUILD_TESTS "Build unit tests" OFF)
option(USE_SYSTEM_YYJSON "Use system yyjson instead of bundled" OFF)
option(USE_SYSTEM_TOMLC99 "Use system tomlc99 instead of bundled" OFF)
option(USE_SYSTEM_SPDLOG "Use system spdlog instead of FetchContent" OFF)

# ── Dependencies ─────────────────────────────────────────────────

# spdlog (header-only)
if(USE_SYSTEM_SPDLOG)
    find_package(spdlog REQUIRED)
else()
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
    )
    set(SPDLOG_BUILD_PIC ON CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

# yyjson (bundled C source)
if(USE_SYSTEM_YYJSON)
    find_package(yyjson REQUIRED)
else()
    # Build bundled yyjson
    add_library(yyjson STATIC third_party/yyjson/yyjson.c third_party/yyjson/yyjson.h)
    target_include_directories(yyjson PUBLIC third_party)
    target_compile_options(yyjson PRIVATE -std=c11 -Wall -Wextra)
endif()

# tomlc99 (bundled C source)
if(USE_SYSTEM_TOMLC99)
    find_package(tomlc99 REQUIRED)
else()
    add_library(tomlc99 STATIC third_party/tomlc99/toml.c third_party/tomlc99/toml.h)
    target_include_directories(tomlc99 PUBLIC third_party)
    target_compile_options(tomlc99 PRIVATE -std=c11 -Wall -Wextra)
endif()

# System libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(DBUS REQUIRED dbus-1)

# ── Source files ─────────────────────────────────────────────────

set(LYRICS_SOURCES
    src/main.cpp
    src/app.cpp
    src/config.cpp
    src/player_linux.cpp
    src/ai/gemini.cpp
    src/ai/openai.cpp
    src/lyrics/provider.cpp
    src/lyrics/lrc_parser.cpp
    src/lyrics/scheduler.cpp
    src/music/music_manager.cpp
    src/music/lrclib.cpp
    src/music/netease.cpp
    src/cache/cache.cpp
    src/ui/gtk_window.cpp
    src/ui/console_tui.cpp
    src/tray/tray_linux.cpp
    src/i3block/controller.cpp
    src/util/http.cpp
    src/util/json.cpp
)

set(LYRICS_HEADERS
    src/app.h
    src/config.h
    src/player.h
    src/ai/ai_interface.h
    src/ai/gemini.h
    src/ai/openai.h
    src/lyrics/provider.h
    src/lyrics/lrc_parser.h
    src/lyrics/scheduler.h
    src/music/music_manager.h
    src/music/lrclib.h
    src/music/netease.h
    src/cache/cache.h
    src/ui/gui.h
    src/ui/console_tui.h
    src/tray/tray.h
    src/i3block/controller.h
    src/util/http.h
    src/util/json.h
    src/util/log.h
)

# ── Executable ───────────────────────────────────────────────────

add_executable(lyrics ${LYRICS_SOURCES} ${LYRICS_HEADERS})

target_include_directories(lyrics PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GTK4_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
)

target_link_libraries(lyrics PRIVATE
    yyjson
    tomlc99
    spdlog::spdlog
    ${GTK4_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${DBUS_LIBRARIES}
)

target_compile_options(lyrics PRIVATE
    -Wall -Wextra -Wpedantic -Wno-unused-parameter
)

# ── Tests ────────────────────────────────────────────────────────

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
