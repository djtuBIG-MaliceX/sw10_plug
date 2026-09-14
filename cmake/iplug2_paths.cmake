# iplug2_paths.cmake — Phase 2 SDK/dependency resolution for the SW10_PLUG CMake build.
#
# User-facing knobs (cache vars; env fallback documented per variable):
#   SW10_IPLUG2_DIR     - iPlug2 submodule root (default: ${CMAKE_SOURCE_DIR}/iPlug2)
#   SW10_VST3_SDK_DIR   - vst3sdk checkout ($ENV{VST3_SDK_DIR}, then the
#                         iPlug2/Dependencies/IPlug/VST3_SDK junction/populated dir)
#   SW10_VST2_SDK_DIR   - vstsdk2.4 root ($ENV{VST2_SDK_DIR}, then the
#                         iPlug2/Dependencies/IPlug/VST2_SDK stub); must locate aeffect.h
#   SW10_CLAP_DIR       - dir containing CLAP_SDK + CLAP_HELPERS
#                         (default: iPlug2/Dependencies/IPlug)
#   SW10_DEPS_WIN_DIR   - prebuilt Skia/Freetype libs (default: iPlug2/Dependencies/Build/win)
#   SW10_ROM_PATH       - ROMSXGM.BIN copied next to every produced binary
#                         (default: ${CMAKE_SOURCE_DIR}/SW10_PLUG/build-win/ROMSXGM.BIN)
#
# Upstream iPlug2 cmake modules resolve SDK paths from IPLUG2_DIR-relative locations
# (Scripts/cmake/VST2.cmake, VST3.cmake, CLAP.cmake). This module maps the SW10_* knobs
# onto those locations: the VST3 junction is (re)created pointing at SW10_VST3_SDK_DIR and
# the two VST2 headers are copied into the VST2_SDK stub when missing. Untracked files
# only — the iPlug2 gitlink is never touched.

# ---------------------------------------------------------------------------
# iPlug2 root
# ---------------------------------------------------------------------------
if(NOT DEFINED SW10_IPLUG2_DIR)
  set(SW10_IPLUG2_DIR "${CMAKE_SOURCE_DIR}/iPlug2" CACHE PATH "iPlug2 submodule root")
endif()
if(NOT EXISTS "${SW10_IPLUG2_DIR}/iPlug2.cmake")
  message(FATAL_ERROR
    "SW10_IPLUG2_DIR='${SW10_IPLUG2_DIR}' is not an iPlug2 tree (iPlug2.cmake missing).\n"
    "Fix: git submodule update --init iPlug2  (or pass -DSW10_IPLUG2_DIR=<path>).")
endif()
# Upstream user-facing knob; FindiPlug2.cmake and all Scripts/cmake modules derive from it.
set(IPLUG2_DIR "${SW10_IPLUG2_DIR}" CACHE PATH "iPlug2 root directory" FORCE)
set(SW10_IPLUG_DEPS_DIR "${IPLUG2_DIR}/Dependencies/IPlug")

# ---------------------------------------------------------------------------
# VST3 SDK
# ---------------------------------------------------------------------------
set(SW10_VST3_SDK_DIR "" CACHE PATH
  "vst3sdk root (tag v3.8.1_build_84). Fallbacks: $ENV{VST3_SDK_DIR}, then iPlug2/Dependencies/IPlug/VST3_SDK")
if(SW10_VST3_SDK_DIR STREQUAL "")
  if(DEFINED ENV{VST3_SDK_DIR} AND NOT "$ENV{VST3_SDK_DIR}" STREQUAL "")
    set(SW10_VST3_SDK_DIR "$ENV{VST3_SDK_DIR}")
  else()
    set(SW10_VST3_SDK_DIR "${SW10_IPLUG_DEPS_DIR}/VST3_SDK")
  endif()
endif()
file(TO_CMAKE_PATH "${SW10_VST3_SDK_DIR}" SW10_VST3_SDK_DIR)

set(SW10_VST3_SDK_LINK "${SW10_IPLUG_DEPS_DIR}/VST3_SDK")
if(NOT EXISTS "${SW10_VST3_SDK_DIR}/public.sdk/source/main/dllmain.cpp")
  message(FATAL_ERROR
    "VST3 SDK incomplete at SW10_VST3_SDK_DIR='${SW10_VST3_SDK_DIR}' (public.sdk/source/main/dllmain.cpp missing).\n"
    "Fix: clone https://github.com/steinbergmedia/vst3sdk at tag v3.8.1_build_84 WITH submodules,\n"
    "  then reconfigure with -DSW10_VST3_SDK_DIR=<path> or set env VST3_SDK_DIR.\n"
    "  This machine: D:/opt/vst/vst3sdk. Do NOT use the legacy 'D:/opt/vst/VST3 SDK' (pre-3.6.6, incompatible).")
endif()
if(EXISTS "${SW10_VST3_SDK_LINK}/public.sdk/source/main/dllmain.cpp")
  if(NOT "${SW10_VST3_SDK_LINK}" STREQUAL "${SW10_VST3_SDK_DIR}")
    message(STATUS "SW10: VST3 SDK is also present at the iPlug2 standard path ${SW10_VST3_SDK_LINK}; upstream cmake modules compile SDK sources from there.")
  endif()
elseif(WIN32)
  # Auto-create the junction upstream cmake modules expect (untracked; needs no admin).
  file(REMOVE_RECURSE "${SW10_VST3_SDK_LINK}")  # removes stub README dir only if empty-ish
  find_program(CMD_EXE cmd.exe)
  execute_process(COMMAND "${CMD_EXE}" /c mklink /J "${SW10_VST3_SDK_LINK}" "${SW10_VST3_SDK_DIR}"
                  RESULT_VARIABLE _sw10_junction_rc OUTPUT_QUIET ERROR_QUIET)
  if(NOT EXISTS "${SW10_VST3_SDK_LINK}/public.sdk/source/main/dllmain.cpp")
    message(FATAL_ERROR
      "Could not populate ${SW10_VST3_SDK_LINK} for the VST3 target (mklink /J rc=${_sw10_junction_rc}).\n"
      "Fix (elevated shell): rmdir \"${SW10_VST3_SDK_LINK}\"  if a stub dir blocks it, then\n"
      "  cmd /c mklink /J \"${SW10_VST3_SDK_LINK}\" \"${SW10_VST3_SDK_DIR}\"\n"
      "  — or run iPlug2/Dependencies/download-vst3-sdk.sh (Git-Bash).")
  endif()
  message(STATUS "SW10: junctioned ${SW10_VST3_SDK_LINK} -> ${SW10_VST3_SDK_DIR}")
else()
  message(FATAL_ERROR
    "VST3 SDK sources must be reachable at ${SW10_VST3_SDK_LINK} (upstream VST3.cmake hardcodes that path).\n"
    "Fix: run iPlug2/Dependencies/download-vst3-sdk.sh.")
endif()
message(STATUS "SW10: VST3 SDK: ${SW10_VST3_SDK_DIR}")

# ---------------------------------------------------------------------------
# VST2 SDK
# ---------------------------------------------------------------------------
set(SW10_VST2_SDK_DIR "" CACHE PATH
  "VST2 SDK (vstsdk2.4) root; must contain aeffect.h (root or pluginterfaces/vst2.x/). Fallbacks: $ENV{VST2_SDK_DIR}, then iPlug2/Dependencies/IPlug/VST2_SDK")
if(SW10_VST2_SDK_DIR STREQUAL "")
  if(DEFINED ENV{VST2_SDK_DIR} AND NOT "$ENV{VST2_SDK_DIR}" STREQUAL "")
    set(SW10_VST2_SDK_DIR "$ENV{VST2_SDK_DIR}")
  else()
    set(SW10_VST2_SDK_DIR "${SW10_IPLUG_DEPS_DIR}/VST2_SDK")
  endif()
endif()
file(TO_CMAKE_PATH "${SW10_VST2_SDK_DIR}" SW10_VST2_SDK_DIR)

# Where the headers actually live (accept both layouts) — "aeffect.h" is the hard requirement.
set(_sw10_vst2_hdr "")
foreach(_cand "${SW10_VST2_SDK_DIR}/pluginterfaces/vst2.x" "${SW10_VST2_SDK_DIR}")
  if(EXISTS "${_cand}/aeffect.h")
    set(_sw10_vst2_hdr "${_cand}")
    break()
  endif()
endforeach()

set(SW10_VST2_STUB_DIR "${SW10_IPLUG_DEPS_DIR}/VST2_SDK")
set(SW10_VST2_SUPPORTED TRUE)
if(_sw10_vst2_hdr)
  # Upstream VST2.cmake compiles against the submodule stub dir; mirror the Phase 1 manual step:
  # drop the two headers (the only ones IPlugVST2.cpp needs) there if missing. Untracked, gitignored.
  if(NOT EXISTS "${SW10_VST2_STUB_DIR}/aeffect.h")
    file(MAKE_DIRECTORY "${SW10_VST2_STUB_DIR}")
    foreach(_h aeffect.h aeffectx.h)
      if(EXISTS "${_sw10_vst2_hdr}/${_h}")
        configure_file("${_sw10_vst2_hdr}/${_h}" "${SW10_VST2_STUB_DIR}/${_h}" COPYONLY)
      endif()
    endforeach()
    message(STATUS "SW10: copied VST2 headers from ${_sw10_vst2_hdr} to ${SW10_VST2_STUB_DIR}")
  endif()
  if(EXISTS "${SW10_VST2_STUB_DIR}/aeffect.h" AND EXISTS "${SW10_VST2_STUB_DIR}/aeffectx.h")
    message(STATUS "SW10: VST2 SDK headers OK (${SW10_VST2_STUB_DIR})")
  else()
    set(SW10_VST2_SUPPORTED FALSE)
  endif()
else()
  if(EXISTS "${SW10_VST2_STUB_DIR}/aeffect.h")
    message(STATUS "SW10: VST2 headers already present in ${SW10_VST2_STUB_DIR}; SW10_VST2_SDK_DIR unused")
  else()
    set(SW10_VST2_SUPPORTED FALSE)
  endif()
endif()
if(NOT SW10_VST2_SUPPORTED)
  if(SW10_BUILD_VST2)
    message(FATAL_ERROR
      "VST2 SDK headers (aeffect.h/aeffectx.h) not found: SW10_VST2_SDK_DIR='${SW10_VST2_SDK_DIR}' has no aeffect.h and ${SW10_VST2_STUB_DIR} is a bare stub.\n"
      "Fix: copy aeffect.h + aeffectx.h from vstsdk2.4\\pluginterfaces\\vst2.x into\n"
      "  ${SW10_VST2_STUB_DIR}\n"
      "  (this machine: D:/opt/vst/vstsdk2.4), or pass -DSW10_VST2_SDK_DIR / set env VST2_SDK_DIR,\n"
      "  or configure with -DSW10_BUILD_VST2=OFF (proprietary SDK cannot run on CI).")
  else()
    message(STATUS "SW10: VST2 SDK absent — VST2 target disabled (SW10_BUILD_VST2=OFF)")
  endif()
endif()

# ---------------------------------------------------------------------------
# CLAP SDK + helpers
# ---------------------------------------------------------------------------
set(SW10_CLAP_DIR "${IPLUG2_DIR}/Dependencies/IPlug" CACHE PATH
  "Directory containing CLAP_SDK and CLAP_HELPERS (run iPlug2/Dependencies/IPlug/CLAP_SDK download scripts via Git-Bash)")
set(SW10_CLAP_SUPPORTED TRUE)
foreach(_d CLAP_SDK CLAP_HELPERS)
  if(NOT EXISTS "${SW10_CLAP_DIR}/${_d}/include")
    set(SW10_CLAP_SUPPORTED FALSE)
    if(SW10_BUILD_CLAP)
      message(FATAL_ERROR
        "CLAP ${_d} missing (expected ${SW10_CLAP_DIR}/${_d}/include).\n"
        "Fix: run iPlug2/Dependencies/download-clap-sdks.sh (Git-Bash), or pass -DSW10_CLAP_DIR=<dir>,\n"
        "  or configure with -DSW10_BUILD_CLAP=OFF.")
    endif()
  endif()
endforeach()
if(SW10_CLAP_DIR STREQUAL "${SW10_IPLUG_DEPS_DIR}")
  # standard location — upstream CLAP.cmake finds it directly
else()
  if(SW10_CLAP_SUPPORTED)
    message(WARNING
      "SW10_CLAP_DIR='${SW10_CLAP_DIR}' differs from the iPlug2 standard location; upstream CLAP.cmake resolves from ${SW10_IPLUG_DEPS_DIR}. "
      "Point CLAP_SDK/CLAP_HELPERS there (junction or download script) if the CLAP target fails to configure.")
  endif()
endif()
if(SW10_CLAP_SUPPORTED)
  message(STATUS "SW10: CLAP SDK + helpers: ${SW10_CLAP_DIR}")
endif()

# ---------------------------------------------------------------------------
# Prebuilt graphics deps (Skia/Freetype) — required only for IGRAPHICS_BACKEND=SKIA
# ---------------------------------------------------------------------------
set(SW10_DEPS_WIN_DIR "${IPLUG2_DIR}/Dependencies/Build/win" CACHE PATH
  "Prebuilt Skia/Freetype lib root (<arch>/<Config>/*.lib from iPlug2/Dependencies/download-prebuilt-libs.sh)")
if(IGRAPHICS_BACKEND STREQUAL "SKIA")
  foreach(_arch x64 Win32)
    if(NOT EXISTS "${SW10_DEPS_WIN_DIR}/${_arch}/Release")
      message(FATAL_ERROR
        "IGRAPHICS_BACKEND=SKIA needs prebuilt libs for ${_arch} at ${SW10_DEPS_WIN_DIR}/${_arch}/Release (missing).\n"
        "Fix: run iPlug2/Dependencies/download-prebuilt-libs.sh win (Git-Bash, needs network),\n"
        "  or pass -DSW10_DEPS_WIN_DIR=<dir>, or use -DIGRAPHICS_BACKEND=NANOVG (default, no download needed).")
    endif()
  endforeach()
  message(STATUS "SW10: Skia libs: ${SW10_DEPS_WIN_DIR}/<arch>/<Config>")
else()
  message(STATUS "SW10: NanoVG/GL2 backend — prebuilt Skia libs not required (${SW10_DEPS_WIN_DIR} may be empty)")
endif()

# ---------------------------------------------------------------------------
# ROM (runtime hard requirement — handleDllPath loads it from the module dir)
# ---------------------------------------------------------------------------
set(SW10_ROM_PATH "${CMAKE_SOURCE_DIR}/SW10_PLUG/build-win/ROMSXGM.BIN" CACHE FILEPATH
  "ROMSXGM.BIN copied post-build next to every produced binary (copyrighted — never committed)")
if(SW10_COPY_ROM AND NOT EXISTS "${SW10_ROM_PATH}")
  message(FATAL_ERROR
    "ROM file not found: SW10_ROM_PATH='${SW10_ROM_PATH}' (required at runtime by SW10_PLUG::handleDllPath).\n"
    "Fix: place ROMSXGM.BIN (untracked, copyrighted) there, or pass -DSW10_ROM_PATH=<path>.\n"
    "  CI has no ROM by design: use the ci-* presets (they set -DSW10_COPY_ROM=OFF) and stage the ROM when installing.")
endif()

message(STATUS "SW10: SDK wiring OK — iPlug2=${IPLUG2_DIR}")
