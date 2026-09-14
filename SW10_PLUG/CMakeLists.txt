# SW10_PLUG API targets (Phase 3). Consumes upstream iPlug2 CMake modules:
# iPlug2::{APP,VST2,VST3,CLAP} carry the per-API sources/defines/entry points,
# ${IGRAPHICS_LIB} is the drawing backend (NanoVG/GL2 by default), and
# iplug_configure_target() applies the per-format output/bundle layout, which is
# then overridden here to build-cmake/<api>/<arch>/<Config>/ with the sln names
# (SW10_PLUG.exe / .dll / .clap / .vst3-bundle).

if(NOT SW10_BUILD_APP AND NOT SW10_BUILD_VST2 AND NOT SW10_BUILD_VST3 AND NOT SW10_BUILD_CLAP)
  message(WARNING "SW10_PLUG: all SW10_BUILD_* options are OFF — no targets created")
endif()

set(SW10_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(SW10_PLUGIN_SOURCES
  "${SW10_PROJECT_DIR}/SW10_PLUG.cpp"
  "${SW10_PROJECT_DIR}/SW10_PLUG.h"
  "${SW10_PROJECT_DIR}/SW10_PLUG_DSP.h"
  "${SW10_PROJECT_DIR}/VLSG.cpp"
  "${SW10_PROJECT_DIR}/VLSG.h"
  "${SW10_PROJECT_DIR}/config.h"
)

# resources/main.rc — same file the vcxprojs compile (dialog/menu/accel IDs are APP-only;
# the plugins embed it just for version-info/icon parity with the sln).
set(SW10_RC_FILE "${SW10_PROJECT_DIR}/resources/main.rc")
set_source_files_properties("${SW10_RC_FILE}" PROPERTIES
  COMPILE_FLAGS "/I\"${SW10_PROJECT_DIR}/resources\" /I\"${SW10_PROJECT_DIR}/resources/fonts\"")

function(sw10_common_setup target)
  target_sources(${target} PRIVATE ${SW10_PLUGIN_SOURCES} "${SW10_RC_FILE}")
  target_include_directories(${target} PRIVATE "${SW10_PROJECT_DIR}" "${SW10_PROJECT_DIR}/resources")
  # Mirror ALL_DEFS from iPlug2/common-win.props exactly: WIN32;_CRT_SECURE_NO_DEPRECATE;
  # _CRT_NONSTDC_NO_DEPRECATE;NOMINMAX (NOMINMAX/_CRT_SECURE_NO_WARNINGS/_CRT_NONSTDC.../_MBCS
  # and NDEBUG|_DEBUG come from iPlug2::IPlug + CMake defaults). API defines + IPLUG_EDITOR/DSP
  # (+ APP's __WINDOWS_DS__/__WINDOWS_MM__/__WINDOWS_ASIO__) come from the iPlug2::<API> target
  # via iplug_configure_target below.
  target_compile_definitions(${target} PRIVATE
    WIN32
    _CRT_SECURE_NO_DEPRECATE
    _CRT_NONSTDC_NO_DEPRECATE
  )
  target_link_libraries(${target} PUBLIC iPlug2::Extras::Synth ${IGRAPHICS_LIB})
  set_property(TARGET ${target} PROPERTY FOLDER "sw10")
endfunction()

# sw10_set_out_dirs(<target> <api> <RUNTIME|LIBRARY>)
# Artifacts: build-cmake/<api>/<arch>/<Config>/, upstream's out/-dir defaults overridden.
function(sw10_set_out_dirs target api kind)
  foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set(dir "${SW10_OUTPUT_ROOT}/${api}/${SW10_ARCH}/${cfg}")
    set_target_properties(${target} PROPERTIES ${kind}_OUTPUT_DIRECTORY_${CFG} "${dir}")
  endforeach()
  set_target_properties(${target} PROPERTIES ${kind}_OUTPUT_DIRECTORY "${SW10_OUTPUT_ROOT}/${api}/${SW10_ARCH}")
endfunction()

# Post-build: ROM next to the binary (hard runtime requirement, handleDllPath);
# optional Program-Files deploy (SW10_POSTBUILD_INSTALL, mirrors postbuild-win.bat).
function(sw10_post_build target api)
  if(SW10_COPY_ROM)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SW10_ROM_PATH}" "$<TARGET_FILE_DIR:${target}>"
      COMMENT "SW10: staging ROMSXGM.BIN next to $<TARGET_FILE_NAME:${target}>")
  endif()
  if(SW10_POSTBUILD_INSTALL)
    if(api STREQUAL "VST2")
      set(dest "$ENV{PROGRAMFILES}/VstPlugins")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dest}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" "${dest}/SW10_PLUG.dll"
        COMMENT "SW10: deploying VST2 to ${dest}")
    elseif(api STREQUAL "VST3")
      set(dest "$ENV{CommonProgramFiles}/VST3")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dest}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
          "$<TARGET_FILE_DIR:${target}>/../.." "${dest}/SW10_PLUG.vst3"
        COMMENT "SW10: deploying VST3 bundle to ${dest}")
    elseif(api STREQUAL "CLAP")
      set(dest "$ENV{CommonProgramFiles}/CLAP")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dest}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${target}>" "${dest}/SW10_PLUG.clap"
        COMMENT "SW10: deploying CLAP to ${dest}")
    endif()
  endif()
endfunction()

# ---------------------------------------------------------------------------
# sw10_app — standalone (IPlugAPP + RTAudio/RTMidi/ASIO come from iPlug2::APP)
# ---------------------------------------------------------------------------
if(SW10_BUILD_APP)
  add_executable(sw10_app WIN32)
  sw10_common_setup(sw10_app)
  iplug_configure_target(sw10_app APP SW10_PLUG)
  sw10_set_out_dirs(sw10_app app RUNTIME)
  sw10_post_build(sw10_app APP)
endif()

# ---------------------------------------------------------------------------
# sw10_vst2 — MODULE .dll (entry VSTPluginMain via dllexport, no .def needed —
# IPlug_include_in_plug_hdr.h EXPORT == __declspec(dllexport), same as sln)
# ---------------------------------------------------------------------------
if(SW10_BUILD_VST2)
  if(IPLUG2_VST2_SUPPORTED)
    add_library(sw10_vst2 MODULE)
    sw10_common_setup(sw10_vst2)
    iplug_configure_target(sw10_vst2 VST2 SW10_PLUG)
    set_target_properties(sw10_vst2 PROPERTIES OUTPUT_NAME "SW10_PLUG" SUFFIX ".dll")
    sw10_set_out_dirs(sw10_vst2 vst2 LIBRARY)
    sw10_post_build(sw10_vst2 VST2)
  else()
    message(FATAL_ERROR "SW10_BUILD_VST2=ON but VST2 SDK headers were not resolved — see cmake/iplug2_paths.cmake messages")
  endif()
endif()

# ---------------------------------------------------------------------------
# sw10_vst3 — bundle layout SW10_PLUG.vst3/Contents/{x86_64-win|x86-win}/SW10_PLUG.vst3
# (SDK sources compiled straight into the target by upstream VST3.cmake — hence /MT
# applies to them via CMAKE_MSVC_RUNTIME_LIBRARY; no vst3sdk add_subdirectory)
# ---------------------------------------------------------------------------
if(SW10_BUILD_VST3)
  if(IPLUG2_VST3_SUPPORTED)
    add_library(sw10_vst3 MODULE)
    sw10_common_setup(sw10_vst3)
    iplug_configure_target(sw10_vst3 VST3 SW10_PLUG)
    set_target_properties(sw10_vst3 PROPERTIES OUTPUT_NAME "SW10_PLUG" SUFFIX ".vst3")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(SW10_VST3_ARCH "x86_64-win")
    else()
      set(SW10_VST3_ARCH "x86-win")
    endif()
    foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
      string(TOUPPER ${cfg} CFG)
      set_target_properties(sw10_vst3 PROPERTIES LIBRARY_OUTPUT_DIRECTORY_${CFG}
        "${SW10_OUTPUT_ROOT}/vst3/${SW10_ARCH}/${cfg}/SW10_PLUG.vst3/Contents/${SW10_VST3_ARCH}")
    endforeach()
    set_target_properties(sw10_vst3 PROPERTIES LIBRARY_OUTPUT_DIRECTORY
      "${SW10_OUTPUT_ROOT}/vst3/${SW10_ARCH}/SW10_PLUG.vst3/Contents/${SW10_VST3_ARCH}")
    # Resources dir for bundle completeness (upstream makes one under out/ — wrong dir
    # once we redirect outputs; recorded in REFACTOR_PLAN.md §6 notes)
    add_custom_command(TARGET sw10_vst3 POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sw10_vst3>/../Resources")
    sw10_post_build(sw10_vst3 VST3)
  else()
    message(FATAL_ERROR "SW10_BUILD_VST3=ON but the VST3 SDK was not resolved — see cmake/iplug2_paths.cmake messages")
  endif()
endif()

# ---------------------------------------------------------------------------
# sw10_clap — MODULE with .clap suffix (clap_entry via CLAP_EXPORT; CLAP SDK is
# header-only, helpers resolved from CLAP_HELPERS/include by iPlug2::CLAP)
# ---------------------------------------------------------------------------
if(SW10_BUILD_CLAP)
  if(IPLUG2_CLAP_SUPPORTED)
    add_library(sw10_clap MODULE)
    sw10_common_setup(sw10_clap)
    iplug_configure_target(sw10_clap CLAP SW10_PLUG)
    set_target_properties(sw10_clap PROPERTIES OUTPUT_NAME "SW10_PLUG" SUFFIX ".clap")
    sw10_set_out_dirs(sw10_clap clap LIBRARY)
    sw10_post_build(sw10_clap CLAP)
  else()
    message(FATAL_ERROR "SW10_BUILD_CLAP=ON but CLAP SDK/helpers were not resolved — see cmake/iplug2_paths.cmake messages")
  endif()
endif()
