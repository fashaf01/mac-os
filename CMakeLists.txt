cmake_minimum_required(VERSION 3.21)
project(MacDock VERSION 0.1.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "MacDock targets Windows only. Configure this project on Windows with Visual Studio 2022.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# A single self-contained .exe with no runtime dependency to install.
option(MACDOCK_STATIC_RUNTIME "Link the C runtime statically" ON)
if(MACDOCK_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_executable(macdock WIN32
    src/main.cpp

    src/core/Log.cpp
    src/core/Settings.cpp

    src/design/Theme.cpp

    src/render/GraphicsDevice.cpp
    src/render/CompositionWindow.cpp
    src/render/TextRenderer.cpp

    src/platform/Dpi.cpp
    src/platform/IconLoader.cpp
    src/platform/AppResolver.cpp
    src/platform/WindowWatcher.cpp
    src/platform/Launcher.cpp
    src/platform/AppBar.cpp
    src/platform/AcrylicBackdrop.cpp
    src/platform/TrayIcon.cpp

    src/dock/DockModel.cpp
    src/dock/DockLayout.cpp
    src/dock/DockWindow.cpp

    res/macdock.rc
)

target_include_directories(macdock PRIVATE src)

target_compile_definitions(macdock PRIVATE
    UNICODE _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_options(macdock PRIVATE
    /W4 /permissive- /utf-8
    $<$<CONFIG:Release>:/O2 /Oi /GL /Gy /fp:fast>
)

target_link_options(macdock PRIVATE
    $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
)

target_link_libraries(macdock PRIVATE
    d3d11 dxgi d2d1 dwrite dcomp windowscodecs
    dwmapi shell32 shlwapi user32 gdi32 ole32 comctl32 advapi32
)

set_target_properties(macdock PROPERTIES OUTPUT_NAME "MacDock")

# ---------------------------------------------------------------- tests
option(MACDOCK_BUILD_TESTS "Build the portable layout/contrast test suites" OFF)

if(MACDOCK_BUILD_TESTS)
    enable_testing()

    add_executable(test_layout tests/test_layout.cpp src/dock/DockLayout.cpp)
    target_include_directories(test_layout PRIVATE src)
    target_compile_definitions(test_layout PRIVATE NOMINMAX)
    add_test(NAME layout COMMAND test_layout)

    # Theme.cpp only touches the registry for the auto light/dark lookup. On
    # Windows it builds against the real headers; elsewhere a tiny shim stands
    # in so the colour maths stays testable on any platform.
    add_executable(test_theme tests/test_theme.cpp src/design/Theme.cpp)
    target_include_directories(test_theme PRIVATE src)
    target_compile_definitions(test_theme PRIVATE NOMINMAX UNICODE _UNICODE)

    if(WIN32)
        target_sources(test_theme PRIVATE src/core/Log.cpp)
        target_link_libraries(test_theme PRIVATE shell32 advapi32)
    else()
        target_sources(test_theme PRIVATE tests/shim/log_stub.cpp)
        target_include_directories(test_theme PRIVATE tests/shim)
    endif()

    add_test(NAME theme COMMAND test_theme)
endif()
