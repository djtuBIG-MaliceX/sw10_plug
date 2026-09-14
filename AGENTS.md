# AGENTS.md — SW10_PLUG

Casio SW-10 (VLSG) synth plugin built on the iPlug2 framework, undergoing a build-system
refactor (MSBuild → CMake). Read this first, then read `REFACTOR_PLAN.md` for the full
phase plan, decisions and risks. **Update the phase checklists in `REFACTOR_PLAN.md` as
you complete work.**

## Repo map

- `SW10_PLUG/` — plugin sources (`SW10_PLUG.cpp/h`, `VLSG.cpp/h`, `config.h`), `projects/`
  (legacy vcxprojs, fallback until CMake proven), `scripts/`, `installer/`, `resources/`.
- `iPlug2/` — git submodule (iPlug2 framework). Pin: see Phase 1 of the plan — currently
  the old `6c639b97f` pin; updating to latest master is part of the refactor.
- `common-win.props`, `SW10_PLUG/config/SW10_PLUG-win.props` — MSBuild macro definitions
  (still the source of truth for defines/include dirs until CMake mirrors them).
- `.github/workflows/` — CI (needs rewrite to CMake; `PROJECT_NAME` is currently wrong).

## Environment (this machine)

- Windows, VS Community 2026, CMake ≥3.25 (4.2.3 installed), PowerShell 7, git, Python 3.14.
- **32-bit builds require the x86 MSVC toolset component installed in VS** — check before
  debugging a "missing cl.exe for Win32" failure.
- SDK locations (never hardcode in CMake; pass via cache vars/env):
  - VST3: `D:\opt\vst\vst3sdk` — vst3sdk `v3.8.1_build_84`, CMake-based, all submodules.
    Do NOT use the legacy `D:\opt\vst\VST3 SDK` folder (pre-3.6.6, incompatible).
  - VST2: `D:\opt\vst\vstsdk2.4` (proprietary — never committed, not on CI).
  - CLAP: `iPlug2/Dependencies/IPlug/CLAP_SDK` + `CLAP_HELPERS` (download scripts in that
    dir fetch them; run via Git-Bash).
  - Skia/Freetype prebuilt libs: `iPlug2/Dependencies/Build/win/<arch>/<config>` — generate
    with `iPlug2/Dependencies/download-prebuilt-libs.sh` (Git-Bash, needs network).

## Hard rules

1. `ROMSXGM.BIN` (Casio ROM, copyrighted, untracked) must sit **next to every built
   binary** at runtime (`SW10_PLUG::handleDllPath` in `SW10_PLUG.cpp` looks in the module
   dir). Local copy: `SW10_PLUG/build-win/ROMSXGM.BIN`. Never commit it, never delete it.
2. Never commit: `build-win/`, `build-cmake/`, `ROMSXGM.BIN`, `aeffect*.h`, VST SDKs,
   secrets. `.gitignore` covers these — keep it that way.
3. Do NOT commit the old `D:\opt\vst\VST3 SDK` path anywhere; CMake must resolve SDKs via
   `SW10_*_DIR` cache vars / env (see REFACTOR_PLAN.md §5).
4. Only update the `iPlug2` submodule via the Phase 1 procedure in the plan; bump the
   submodule pointer in a dedicated commit at repo root, never accidentally.
5. Do not delete/retire vcxprojs, the `.sln`, macOS/iOS/WAM projects, or AAX project files
   without explicit instruction — sln is the fallback (AAX just goes unmaintained).
6. Preserve XP/Win7 32-bit compatibility intent: keep `-xp` CMake presets working where
   feasible; if a dependency (likely Skia) forces Win7+, record the decision in
   REFACTOR_PLAN.md §2 rather than silently dropping support.

## Build commands (target state — see plan phases for current reality)

```powershell
# configure + build one API/arch/config
cmake --preset vs-x64          # CMakePresets.json; sets -DSW10_VST3_SDK_DIR etc.
cmake --build --preset vs-x64-release
# or full matrix
cmake --preset vs-win32; cmake --build --preset vs-win32-release
cmake --preset vs-x64;   cmake --build --preset vs-x64-release
```

Outputs land in `build-cmake/<api>/<arch>/<Config>/` with `ROMSXGM.BIN` copied alongside.
Toggles: `-DSW10_BUILD_{APP,VST2,VST3,CLAP}=ON|OFF` (AAX is not a CMake target).

Legacy fallback (until CMake is done): `msbuild SW10_PLUG\SW10_PLUG.sln /p:Configuration=Release /p:Platform=x64`
(needs prebuilt libs + VST2 SDK in `iPlug2/Dependencies/IPlug/VST2_SDK`, see Phase 1).

## Plugin facts that affect builds

- Graphics for the sln fallback is **NanoVG+GL2** (`EXTRA_ALL_DEFS=IGRAPHICS_NANOVG;IGRAPHICS_GL2` in
  `SW10_PLUG-win.props`; app imports opengl32 — the new pin embeds the NanoVG impl inside `IGraphicsWin.cpp`,
  its `Drawing/*.cpp` items stay excluded in every project). Skia is the Phase-3 CMake option: linking then needs
  `skia/skparagraph/skshaper/skunicode_core/skunicode_icu` (+svg/libpng/zlib/freetype) from
  `iPlug2/Dependencies/Build/win/<arch>/<config>` (`skunicode` is split core+icu in the current deps zip).
- Instrument plugin (`PLUG_TYPE 1`), MIDI-in, MPE, **no state chunks**; static `/MT` CRT
  is the existing convention.
- API macros per target: `APP_API`+`__WINDOWS_DS__/__WINDOWS_MM__/__WINDOWS_ASIO__`,
  `VST2_API`, `VST3_API`, `CLAP_API`, always with `IPLUG_EDITOR=1;IPLUG_DSP=1`,
  plus `NOMINMAX` and the `_CRT_SECURE_NO*_DEPRECATE` trio — mirror exactly when
  porting defines to CMake (`common-win.props` is the reference).
- CLAP builds via sln since Phase 1 (submodule `iPlug2/common-win.props` supplies `CLAP_DEFS`/`CLAP_INC_PATHS`;
  legacy deploy macros shimmed in `SW10_PLUG-win.props` — see REFACTOR_PLAN §3b).

## Workflow for code changes

- Keep edits surgical; prefer changing `SW10_PLUG/` sources over vendored `iPlug2/` code.
  Any iPlug2 tree edits must be upstreamable or documented in REFACTOR_PLAN.md (tree edits
  are lost on submodule checkout otherwise).
- After touching build files, verify both VS generator archs configure:
  `cmake -B b32 -A Win32 && cmake -B b64 -A x64` (delete b32/b64 after).
- Smoke-test: app exe launches with UI; plugins load in REAPER (paths in old
  `common-win.props`); UI renders = Skia link + ROM load OK.

## Status

See phase checklists in `REFACTOR_PLAN.md`. Done so far:
- VST3 SDK clone to `D:\opt\vst\vst3sdk` (v3.8.1_build_84).
- **Phase 0 complete.** Prebuilt Skia/Freetype libs downloaded to `iPlug2/Dependencies/Build/win/...`;
  `iPlug2/Dependencies/IPlug/VST3_SDK` junctioned to `D:\opt\vst\vst3sdk`; XP diff saved to
  `SW10_PLUG/docs/xp-experiment.diff` and vst2.vcxproj reverted.
- **Phase 1 complete (2026-09-15).** iPlug2 bumped `6c639b97f` → `d54f69050` (gitlink commit `aa9dad9`); junction +
  prebuilt libs + CLAP SDKs regenerated; upstream props merged (`SW10_PLUG-win.props` legacy-macro shim; root
  `common-win.props` is orphaned — the live one is `iPlug2/common-win.props`); `WDL/win32_utf8.c` added to all four
  vcxprojs (required by new pin). Upstream now ships CMake (`iPlug2/Scripts/cmake/`, `iPlug2.cmake`) — read before
  writing Phase 3 from scratch. Details: plan §4/§3b.
- **MSBuild fallback builds app/vst2/vst3/clap in Win32 AND x64 Release on the new pin** (AAX not built, no SDK — dropped).
  App smoke-tested (window + ROM + UI) on both archs. Needed: `/p:PlatformToolset=v145` (v143 compiler absent;
  v141/14.16 *is* installed — the "no XP toolset" note below is wrong, see plan §3) and an in-repo Unicode fix
  in `SW10_PLUG.cpp::handleDllPath` (`GetModuleFileNameA`). See plan §3/§3b.
- Still to do: Phase 2/3 CMake (nothing CMake yet), Phase 4 host-load matrix, Phase 5 CI.
