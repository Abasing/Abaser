# ============================================================================
#  Abaser Clicker — the standalone auto-clicker (Win32 + Dear ImGui + DX9)
# ----------------------------------------------------------------------------
#  Builds a single self-contained AbaserClicker.exe. Two supported toolchains:
#
#    MSVC (Windows):      cmake -S exe -B build -DABASER_VERSION="x.y"
#                           cmake --build build --config Release
#    MinGW-w64 (cross):   cmake -S exe -B build \
#                           -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
#                           -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
#                           -DABASER_VERSION="x.y"
#                         cmake --build build
#
#  The release pipeline builds it with MinGW-w64 on Linux and ships it next
#  to the mod jar in every release.
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(AbaserClicker LANGUAGES C CXX)

set(ABASER_VERSION "dev" CACHE STRING "Version string shown in the UI")

add_executable(AbaserClicker WIN32
    src/main.cpp
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_dx9.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
)

target_include_directories(AbaserClicker PRIVATE
    src
    third_party/imgui
    third_party/imgui/backends
)

target_compile_definitions(AbaserClicker PRIVATE
    ABASER_VERSION="${ABASER_VERSION}"
)

target_compile_features(AbaserClicker PRIVATE cxx_std_17)

if(MSVC)
    # Fully static CRT -> one self-contained exe, no VC++ runtime needed.
    set_property(TARGET AbaserClicker PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    target_compile_options(AbaserClicker PRIVATE /W3)
else()
    # MinGW: static-link everything (libgcc, libstdc++, threads).
    # -mwindows is normally added by CMake's Windows-GNU platform rules for
    # WIN32 executables, but pass it explicitly too so the exe can never
    # ship with a console flash even under a sloppy configure.
    target_compile_options(AbaserClicker PRIVATE -O2 -Wall)
    target_link_options(AbaserClicker PRIVATE -mwindows -static -static-libgcc -static-libstdc++)
endif()

# d3d9: the renderer; dwmapi/gdi32/imm32/user32: the ImGui Win32 backend
# (dwmapi is referenced by ImGui_ImplWin32_EnableAlphaCompositing even if
# unused -- the backend's own header note says MinGW must link it).
target_link_libraries(AbaserClicker PRIVATE d3d9 dwmapi gdi32 imm32 user32)
