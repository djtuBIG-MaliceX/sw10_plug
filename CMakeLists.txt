# SW10_PLUG — CMake build (Windows; replaces the MSBuild sln, which stays as fallback).
# Quickstart: cmake --preset vs-x64 && cmake --build --preset vs-x64-release
# Consumes the iPlug2 submodule's own CMake support (iPlug2.cmake + Scripts/cmake/*);
# see REFACTOR_PLAN.md §5/§6 for SDK wiring and target design.
cmake_minimum_required(VERSION 3.25)
project(SW10_PLUG VERSION 0.0.3 LANGUAGES C CXX)

if(NOT WIN32 OR NOT (MSVC OR MINGW))
  message(FATAL_ERROR
    "The SW10_PLUG CMake build targets Windows with either the Visual Studio generator "
    "(e.g. 'Visual Studio 18 2026', -A Win32|x64) or the MinGW-w64 (MSYS2 MINGW64, x86_64) "
    "GCC/Clang toolchain via the 'Ninja' generator. Use the legacy .vcxproj/.xcodeproj "
    "projects for other platforms/toolchains.")
endif()

if(MSVC)
  set(SW10_TOOLCHAIN "msvc")
elseif(MINGW)
  set(SW10_TOOLCHAIN "mingw")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options (all four API targets ON by default; AAX is intentionally absent — plan §2.3)
# ---------------------------------------------------------------------------
option(SW10_BUILD_APP  "Build the standalone app (sw10_app)"          ON)
option(SW10_BUILD_VST2 "Build the VST2 plugin (sw10_vst2)"            ON)
option(SW10_BUILD_VST3 "Build the VST3 plugin (sw10_vst3)"            ON)
option(SW10_BUILD_CLAP "Build the CLAP plugin (sw10_clap)"            ON)
option(SW10_COPY_ROM   "Post-build: copy ROMSXGM.BIN next to each binary (runtime requirement)" ON)
option(SW10_POSTBUILD_INSTALL "Post-build: also deploy to Program Files plugin dirs (old postbuild-win.bat behavior)" OFF)

# ---------------------------------------------------------------------------
# Phase 2 — SDK / dependency resolution (SW10_* cache vars + clear errors)
# ---------------------------------------------------------------------------
include(cmake/iplug2_paths.cmake)

# ---------------------------------------------------------------------------
# iPlug2 consumption. We include iPlug2.cmake + find_package(iPlug2) the way upstream
# Examples do, instead of add_subdirectory(iPlug2) (which would pull in Examples/Tests).
# Graphics: NanoVG/GL2 — the upstream Windows default AND what the proven sln build uses
# (deviation from plan §6 "Skia", recorded in REFACTOR_PLAN.md §6 notes).
# ---------------------------------------------------------------------------
set(IGRAPHICS_BACKEND "NANOVG" CACHE STRING "IGraphics drawing backend (NANOVG or SKIA)")
set_property(CACHE IGRAPHICS_BACKEND PROPERTY STRINGS "NANOVG" "SKIA")
set(IGRAPHICS_RENDERER "GL2" CACHE STRING "IGraphics renderer")

# Upstream Deploy.cmake defaults (out/-relative paths) do not match our output layout;
# deployment is handled by SW10_POSTBUILD_INSTALL below.
set(IPLUG_DEPLOY_PLUGINS OFF CACHE BOOL "iPlug2 auto-deploy (unused; SW10_POSTBUILD_INSTALL replaces it)" FORCE)

include(${IPLUG2_DIR}/iPlug2.cmake)
find_package(iPlug2 REQUIRED)

# MinGW-w64 (MSYS2) compatibility: remap the MSVC import-library names the upstream
# iPlug2 INTERFACE targets hardcode, and add static-runtime / big-obj flags. No-op on MSVC.
include(mingw_compat)

# Static /MT CRT everywhere (matches common-win.props; upstream sets the same default).
# MSVC-only knob — MinGW static runtime is handled in cmake/mingw_compat.cmake.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
      CACHE STRING "static CRT for all targets incl. compiled VST3 SDK sources" FORCE)
endif()

# ---------------------------------------------------------------------------
# Output layout: build-cmake/<api>/<arch>/<Config>/ with the sln artifact names
# ---------------------------------------------------------------------------
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(SW10_ARCH "x64")
else()
  set(SW10_ARCH "Win32")
endif()
# Keep MinGW artifacts in their own <arch>-mingw dir so they never collide with the
# MSVC build in the shared build-cmake root (output dir segment only; SDK arch logic
# in SW10_PLUG/CMakeLists.txt keys off CMAKE_SIZEOF_VOID_P, not this variable).
# Clang gets its own -clang suffix too so the two MinGW toolchains do not clobber each other.
if(MINGW)
  set(SW10_ARCH "${SW10_ARCH}-mingw")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    string(APPEND SW10_ARCH "-clang")
  endif()
endif()
set(SW10_OUTPUT_ROOT "${CMAKE_SOURCE_DIR}/build-cmake" CACHE PATH
    "Artifact root; outputs land in <root>/<api>/${SW10_ARCH}/<Config>/")

add_subdirectory(SW10_PLUG)

message(STATUS "SW10_PLUG configure summary:")
message(STATUS "  toolchain=${SW10_TOOLCHAIN}  arch=${SW10_ARCH}  graphics=${IGRAPHICS_BACKEND}/${IGRAPHICS_RENDERER}")
message(STATUS "  app=${SW10_BUILD_APP} vst2=${SW10_BUILD_VST2} vst3=${SW10_BUILD_VST3} clap=${SW10_BUILD_CLAP} (AAX dropped)")
message(STATUS "  ROM copy=${SW10_COPY_ROM} (${SW10_ROM_PATH})")
message(STATUS "  outputs -> ${SW10_OUTPUT_ROOT}/<api>/${SW10_ARCH}/<Config>/")
