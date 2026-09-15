# SW-10 PLUG
Casio SW-10 in a VSTi/CLAP/whatever, using iPlug2 framework.
Still highly experimental because this thing was never designed to be run with such low buffer sizes.
Invasive modifications also being done on `VLSG.c` code to improve/fix MIDI playback for listening usage.

# Build (CMake — primary, VS 2026 + CMake >= 3.25)

```powershell
cmake --preset vs-x64      ; cmake --build --preset vs-x64-release    # x64
cmake --preset vs-win32    ; cmake --build --preset vs-win32-release  # Win32
```

Outputs land in `build-cmake/<api>/<arch>/<Config>/` (`SW10_PLUG.exe` / `.dll` / `.clap` /
`SW10_PLUG.vst3/Contents/x86_64-win| x86-win/`). Targets: app, VST2, VST3, CLAP — toggles
`-DSW10_BUILD_{APP,VST2,VST3,CLAP}=OFF` (AAX is dropped). Graphics backend: NanoVG/GL2
(upstream default; `-DIGRAPHICS_BACKEND=SKIA` possible once `iPlug2/Dependencies/download-prebuilt-libs.sh`
ran). CRT is static `/MT`; `ROMSXGM.BIN` is copied next to every binary automatically
(`SW10_ROM_PATH`, default `SW10_PLUG/build-win/ROMSXGM.BIN`).

SDK discovery via cache vars / env (never hardcoded): `SW10_VST3_SDK_DIR` / `VST3_SDK_DIR`
(vst3sdk v3.8.1_build_84; auto-junctioned into `iPlug2/Dependencies/IPlug/VST3_SDK`),
`SW10_VST2_SDK_DIR` / `VST2_SDK_DIR` (vstsdk2.4 — only `aeffect.h`/`aeffectx.h` are needed and
are auto-copied into the submodule stub), CLAP SDKs in `iPlug2/Dependencies/IPlug/CLAP_SDK|CLAP_HELPERS`
(`download-clap-sdks.sh`, Git-Bash). Configure errors name the exact fix when something is missing.

Load-test without a DAW: `pwsh SW10_PLUG/scripts/loadtest.ps1 -Arch x64|Win32`.
One-shot distribution script: `SW10_PLUG/scripts/makedist-win.bat` (CMake + installer; add `-Legacy`
for the old msbuild flow).

## Build (MinGW-w64 — MSYS2 MINGW64, GCC + Clang, x86_64)

Run from an **MSYS2 MINGW64** shell (needs `mingw-w64-x86_64-{gcc,clang,cmake,ninja}`; the Clang
preset also needs `mingw-w64-x86_64-clang`). The same SDK/ROM discovery as above applies.

```sh
cmake --preset mingw-x64       ; cmake --build --preset mingw-x64-release   # GCC
cmake --preset mingw-clang-x64 ; cmake --build --preset mingw-clang-x64-release  # Clang

# load-test each toolchain (artifacts live in build-cmake/<api>/x64-mingw[-clang]/Release/):
pwsh SW10_PLUG/scripts/loadtest.ps1 -Arch x64 -ArchDir x64-mingw         # GCC
pwsh SW10_PLUG/scripts/loadtest.ps1 -Arch x64 -ArchDir x64-mingw-clang   # Clang
```

All four targets (app/VST2/VST3/CLAP) build under both compilers, statically linked (no
`libgcc`/`libstdc++`/`winpthread` DLL deps). x86_64 only — 32-bit MinGW would need the
separate `mingw-w64-i686` toolchain. Everything is gated behind `MINGW`/`Clang` checks in
`cmake/mingw_compat.cmake` + `cmake/mingw_portability_prelude.h`, so the MSVC build is unchanged
and the `iPlug2` submodule is never edited. Details: `REFACTOR_PLAN.md` §6b.

## Not included in repo (find it yourself)
- ROMSXGM.BIN (Copyrighted Casio ROM) — required next to every binary at runtime
- VST 2.x SDK (Thanks Steinberg)


# Credits
[M-HT](https://github.com/M-HT/casio_sw-10) and [putara](https://github.com/putara/casio_sw-10) for the reverse-engineered VLSG code.
more names will appear here.