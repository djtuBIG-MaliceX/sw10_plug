# mingw_compat.cmake — MinGW-w64 (MSYS2) compatibility layer for the SW10_PLUG CMake build.
#
# Included from the root CMakeLists.txt AFTER find_package(iPlug2) when the compiler is
# the Windows GNU/Clang (MinGW) toolchain. The MSVC path is untouched by everything here.
#
# Scope (see REFACTOR_PLAN.md MinGW notes):
#   - x86_64 (MINGW64) only; a 32-bit MinGW build needs the i686 toolchain and is not wired.
#   - Graphics backend NanoVG/GL2 (the default). NanoVG compiles nanovg.c+glad.c from
#     source inside IGraphicsWin.cpp and glad LoadLibrary()s opengl32.dll at runtime, so
#     NO prebuilt MSVC graphics libs are linked (unlike the SKIA backend).
#
# Everything here stays in the superproject; the iPlug2 submodule gitlink is never touched.

if(NOT MINGW)
  return()
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR
    "SW10_PLUG MinGW support is x86_64-only (MSYS2 'MSYS2 MINGW64' shell).\n"
    "A 32-bit MinGW build needs the separate mingw-w64-i686 toolchain and is not configured here.")
endif()

# ---------------------------------------------------------------------------
# Static runtime + COFF section fixes.
#   -static-libgcc/-static-libstdc++ : plugins must not depend on libgcc/libstdc++ DLLs
#                                      (matches the MSVC /MT self-contained CRT intent).
#   -Wa,-mbig-obj                    : IGraphicsWin.cpp unity-builds nanovg.c+glad.c into
#                                      one huge TU; without this the COFF assembler hits
#                                      "too many sections".
#   -fpermissive                     : iPlug/APP glue assigns FARPROC (GetProcAddress) to
#                                      void* implicitly — valid-permissive MSVC, hard error
#                                      under libstdc++. Downgrades it back to a warning.
#   -include <prelude>               : libstdc++ does not pull <memory>/<cmath>/... in
#                                      transitively the way MSVC's <Windows.h>+SDK headers
#                                      happen to; force-include a small STL prelude into every
#                                      C++ TU (iPlug SDK + VST3 SDK + plugin sources) so the
#                                      upstream headers that use std::unique_ptr etc. resolve.
# COMPILE_LANGUAGE guards keep windres (RC) and plain-C TUs out of the C++-only flags.
# ---------------------------------------------------------------------------
add_compile_definitions(_USE_MATH_DEFINES)          # M_PI / M_PI_2 under __STRICT_ANSI__
add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-Wa,-mbig-obj>")
add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:-fpermissive>")
add_link_options(-static -static-libgcc -static-libstdc++)
string(APPEND CMAKE_CXX_FLAGS
  " -include \"${CMAKE_CURRENT_LIST_DIR}/mingw_portability_prelude.h\"")

# IGraphicsWin.cpp calls the WGL/GDI entry points (wglCreateContext/wglMakeCurrent/SwapBuffers/…)
# directly. MSVC resolves them via opengl32.lib/gdi32.lib on the project link line; GCC ignores
# #pragma comment(lib, …) so we link them explicitly for every editor target (unused ones are
# dropped by the linker on the odd target that does not pull them in).
link_libraries(opengl32 gdi32)

# ---------------------------------------------------------------------------
# sw10_mingw_fixup_imported_libs() — the upstream iPlug2 INTERFACE targets
# (iPlug2::IPlug, iPlug2::APP, iPlug2::Extras::OSC) list MSVC import-library names
# (Shlwapi.lib, comctl32.lib, wininet.lib, dsound.lib, winmm.lib, ws2_32.lib). MinGW
# ships lib<name>.a and wants -l<name>, and ld will NOT find a "Foo.lib". Rewrite those
# bare *.lib entries in the imported targets' INTERFACE_LINK_LIBRARIES to plain names
# (CMake then emits -lfoo). Real file paths (skia.lib / WebView2LoaderStatic.lib) are
# left alone — those live behind backends/features the NanoVG MinGW build doesn't use.
# ---------------------------------------------------------------------------
function(sw10_mingw_fixup_lib_names target)
  if(NOT TARGET ${target})
    return()
  endif()
  get_target_property(_libs ${target} INTERFACE_LINK_LIBRARIES)
  if(NOT _libs)
    return()
  endif()
  set(_rewritten "")
  set(_changed FALSE)
  foreach(_lib IN LISTS _libs)
    # Only bare "Name.lib" tokens (no directory, no generator expression, no path).
    if(_lib MATCHES "^([A-Za-z0-9_]+)\.lib$")
      list(APPEND _rewritten "${CMAKE_MATCH_1}")
      set(_changed TRUE)
    else()
      list(APPEND _rewritten "${_lib}")
    endif()
  endforeach()
  if(_changed)
    set_target_properties(${target} PROPERTIES INTERFACE_LINK_LIBRARIES "${_rewritten}")
    message(STATUS "SW10/mingw: rewrote MSVC .lib names in ${target} -> MinGW -l names")
  endif()
endfunction()

foreach(_t iPlug2::IPlug iPlug2::APP iPlug2::Extras::OSC iPlug2::Extras::Synth iPlug2::VST3 iPlug2::VST2 iPlug2::CLAP iPlug2::IGraphics)
  sw10_mingw_fixup_lib_names(${_t})
endforeach()
