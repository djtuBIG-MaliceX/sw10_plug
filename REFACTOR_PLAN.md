# SW10_PLUG Refactor Plan

Goal: modernize the build system — update iPlug2 to latest master, replace MSBuild with
CMake for the four Windows targets (standalone app, VST2, VST3, CLAP) in both Win32 and
x64, preserve XP/Win7 32-bit compatibility, and keep the existing `.sln` as a fallback
until CMake is proven.

Companion file: `AGENTS.md` (operational instructions for AI agents) references this plan.
Track phase progress in the phase checklists below — agents must update checkmarks as work lands.

---

## 1. Current state (verified 2026-09-15)

### Repo layout (`sw10_plug` = git root)

| Item | Detail |
|---|---|
| Plugin sources | `SW10_PLUG/SW10_PLUG.{h,cpp}`, `SW10_PLUG/VLSG.{h,cpp}`, `SW10_PLUG/config.h` (version 0.0.3, `PLUG_TYPE 1` = instrument, MPE, no state chunks) |
| Win build | `SW10_PLUG/SW10_PLUG.sln` + `projects/*.vcxproj`: `app`, `vst2`, `vst3`, `clap`, `aax` × Win32/x64 × Debug/Release/Tracer |
| Property system | root `common-win.props` → `SW10_PLUG/config/SW10_PLUG-win.props` (imported by every vcxproj). NOTE: local `common-win.props` predates CLAP support — defines no `CLAP_DEFS`/`CLAP_INC_PATHS`; the clap vcxproj references them, so CLAP via sln is currently broken. Upstream iPlug2 `common-win.props` has them. |
| Scripts | `SW10_PLUG/scripts/makedist-win.bat` (hardcodes VS2019 `vcvarsall.bat` path, msbuild, InnoSetup, 7zip), `postbuild-win.bat`, `prebuild-win.bat`, `prepare_resources-win.py` |
| Graphics | IGraphics **NanoVG + GL2** for the `.sln` fallback (`EXTRA_ALL_DEFS=IGRAPHICS_NANOVG;IGRAPHICS_GL2` in `SW10_PLUG-win.props`; app imports OPENGL32 — verified via baseline exe/PDB). The `IGraphicsNanoVG.cpp`/`IGraphicsSkia.cpp` ClCompile items are excluded in every config — correct for the new pin, which embeds the NanoVG impl inside `IGraphicsWin.cpp` (`#include "IGraphicsNanoVG.cpp"` ~line 2226, + `nanovg.c`). The earlier "Skia is the only backend" claim (AGENTS.md/§1) was **wrong** for the sln path; Skia remains an option for Phase 3 (new-master `IGraphicsSkia.cpp` pragma-links `skia,skparagraph,skshaper,skunicode_core,skunicode_icu,opengl32` — note `skunicode` is now **split** core+icu, matching the re-downloaded deps zip). |
| Runtime data | `ROMSXGM.BIN` (Casio ROM, NOT in git; copy exists at `SW10_PLUG/build-win/ROMSXGM.BIN`). Loaded at runtime from the module's directory via `SW10_PLUG::handleDllPath` (see `SW10_PLUG.cpp`) → **every** build output dir must have the ROM next to the binary. |
| iPlug2 submodule | **Updated 2026-09-15 (Phase 1)**: `6c639b97f` (v0.3.0-864, 2024-05) → `d54f69050` (tip of `origin/master`, 2026-08-19, 944 commits ahead of old pin; past `v1.0.0-beta`). Root commit `aa9dad9` holds the gitlink. Upstream **now has CMake support** (`CMakeLists.txt`, `CMakePresets.json`, `iPlug2.cmake`, `Scripts/cmake/*` incl. `FindiPlug2.cmake`, `VST2/VST3/CLAP/APP.cmake`) — study/reuse in Phase 3 instead of hand-rolling source lists; upstream Examples dropped their vcxprojs, so our `.sln` is self-maintained from here on. |
| Dirty tree | `SW10_PLUG/projects/SW10_PLUG-vst2.vcxproj` modified: Release→`ClangCL`, `WindowsTargetPlatformVersion 10.0`→`7.0` (XP-era experiment; preserve the learning, revert the file). |
| Hygiene | `build-win/` (incl. objs, dll/exe, `ROMSXGM.BIN`) is tracked in git despite `.gitignore` (`build-*`, `ROMSXGM.BIN`, `aeffect*.h` ignored — files added before the rules). `.github/workflows/build-native.yml` has `PROJECT_NAME: TemplateProject` (wrong) and msbuild-only steps. |
| Stale artifacts | `SW10_PLUG/.vs/`, `build-win/` committed; `iPlug2OOS.code-workspace` at root (template leftover). |

### SDKs

| SDK | Status | Location |
|---|---|---|
| VST3 | **DONE** — freshly cloned `v3.8.1_build_84` (all submodules: base, pluginterfaces, public.sdk, cmake, vstgui4, doc, tutorials). SDK CMake still has Win32 generator-platform branches (`cmake/modules/SMTG_AddSMTGLibrary.cmake`), so 32-bit VST3 should still build with `-A Win32` (verify). | `D:\opt\vst\vst3sdk` |
| VST3 (old) | pre-3.6.6 (no `ucmd.h`, no CMake, `bin/Windows 32 bit`, `vstgui.sf`) — incompatible with current iPlug2. Leave in place; do NOT reference. | `D:\opt\vst\VST3 SDK` |
| VST2 | `vstsdk2.4` present. **Verified (Phase 1)**: only `aeffect.h`/`aeffectx.h` are needed by the VST2 glue. The submodule's `iPlug2/Dependencies/IPlug/VST2_SDK` dir tracks only a README; the two headers sit there as untracked local copies (previously placed) and satisfied the vst2 build, so no `/p:VST2_SDK` override was needed. Fresh clones: copy from `D:\opt\vst\vstsdk2.4` (or pass `/p:VST2_SDK`). | `D:\opt\vst\vstsdk2.4` (and copies in submodule stub dir) |
| CLAP | **Refreshed 2026-09-15 (Phase 1)** via `download-clap-sdks.sh` (Git-Bash): clap SDK `1.2.1→1.2.10`, helpers at main. New glue (`IPlugCLAP.cpp/.h`) includes `plugin.hxx`/`plugin.hh` — resolved from `CLAP_HELPERS\include\clap\helpers` (the new props put that dir directly on the include path). | `iPlug2/Dependencies/IPlug/CLAP_SDK` + `CLAP_HELPERS` |
| ASIO | headers ship with RTAudio (`iPlug2/Dependencies/IPlug/RTAudio/include`). | — |
| AAX | Not owned. **Decision: drop AAX support** (out of CMake scope; leave unmaintained). | — |

### Local toolchain

- VS Community 2026 (MSVC v145-era toolset; **no v141_xp** — must be installed explicitly, see Phase 3/XP), Windows 10/11 SDK auto.
- CMake 4.2.3 (minimum: pin `cmake_minimum_required(VERSION 3.25)` — required by vst3sdk).
- Python 3.14, git 2.53. Shell: PowerShell 7.
- Note: 32-bit MSVC (`cl.exe` x86) is an optional VS component now — verify `cl -Bp140`/Win32 compile works before Phase 3.

---

## 2. Decisions (agreed)

1. **VST3 SDK**: new vst3sdk clone at `D:\opt\vst\vst3sdk`, pinned tag (✅ done). Never use the old `D:\opt\vst\VST3 SDK`.
2. **iPlug2**: update submodule to latest `origin/master` (`f071a6503` or newer tip at time of work).
3. **CMake covers**: standalone app, VST2, VST3, CLAP × Win32+x64. **Drop AAX.** Keep vcxprojs + sln untouched and working as fallback; retire only after CMake is proven and by explicit decision. macOS/iOS/Web(WAM) projects stay as-is.
4. **XP/Win7**: retain 32-bit XP-era compatibility as a goal. Modern presets build Win32 with the current toolset; separate `-xp` presets/toolsets attempt XP. **Decision gate (Phase 3):** if prebuilt Skia/Freetype can't run on XP (likely — built with modern CRT), scope XP shipping to VST2/Win32 (+app?) and document Win7 as the floor for Skia targets. Win7 x86 works with modern CRT (no XP CRT) — easy win even if XP is dropped.

---

## 3. Phase 0 — Baseline & repo hygiene

- [x] Save the XP experiment: `git diff -- SW10_PLUG/projects/SW10_PLUG-vst2.vcxproj` → `SW10_PLUG/docs/xp-experiment.diff` (also in §10 appendix), then `git checkout -- SW10_PLUG/projects/SW10_PLUG-vst2.vcxproj` (done).
- [x] Repo hygiene: verified `git ls-files` shows **0 tracked** files under `SW10_PLUG/build-win` or `SW10_PLUG/.vs` — they were *already* untracked (ignore rules cover them); `git rm --cached` was a no-op. `.gitignore` covers `build-*`, `ROMSXGM.BIN`, `aeffect*.h`. Nothing to change.
- [x] Baseline build (see notes below): after wiring deps, `msbuild SW10_PLUG.sln /p:Configuration=Release /p:Platform=x64|Win32` now **builds all four targets** (app, vst2, vst3, clap) in **both archs**. Only **AAX** fails (no SDK — being dropped, §2.3). This is *ahead of* the plan's expected baseline.
- [x] `ROMSXGM.BIN` present at `SW10_PLUG/build-win/ROMSXGM.BIN`; copied next to every produced binary in both archs (post-build steps in Phase 3 will automate via `SW10_ROM_PATH`).

### Phase 0 baseline results (2026-09-15, VS 2026 / MSBuild, toolset forced `/p:PlatformToolset=v145`)

Setup performed to get a meaningful baseline (the old pin can't build without these):
- Ran `iPlug2/Dependencies/download-prebuilt-libs.sh win` (Git-Bash, network) → populated
  `iPlug2/Dependencies/Build/win/{Win32,x64}/{Debug,Release}` with the 10 static libs
  (`skia,svg,skshaper,skunicode,libpng,zlib,freetype,skottie,skparagraph,sksg`) +
  `Build/src/{skia,freetype}` headers.
- **Junction** `iPlug2/Dependencies/IPlug/VST3_SDK` → `D:\opt\vst\vst3sdk` (the vst3 vcxproj compiles SDK
  sources via that literal relative path; the submodule dir only tracked a stub README). See Deviations.
- **Fixed a Unicode bug** in `SW10_PLUG/SW10_PLUG.cpp::handleDllPath`: `GetModuleHandleEx`/`GetModuleFileName`
  → explicit `…ExA`/`…FileNameA`. The VST3 SDK forces `UNICODE`, so the generic macros mapped to the `W`
  overloads and rejected the `char[MAX_PATH]` buffers (only vst3 hit it; app/vst2/clap aren't `UNICODE`).
  Persistent, in-repo fix.

| Target | Win32 build | x64 build | Smoke (app: window+ROM+UI) |
|---|---|---|---|
| app (exe) | ✅ | ✅ | ✅ launches, window "SW10_PLUG", stable (both archs) |
| vst2 (dll) | ✅ | ✅ | pending host load |
| vst3 | ✅ | ✅ | pending host load |
| clap | ✅ | ✅ | pending host load |
| aax | ❌ no SDK | ❌ no SDK | dropped (§2.3) |

- `MSBuild.exe`: `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe`
  (via vswhere). Installed toolsets on box: v141 (MSVC 14.16.27023) and v145 (MSVC 14.51.36231); v143
  compiler binaries **not** present, hence the project's `v143`/`v141_xp` toolset is overridden to `v145`
  for the baseline. NOTE: v141 (14.16) **is** present — the §1 "no v141_xp" assumption is wrong; XP
  toolset availability is a Phase 3 investigation, not a blocker.
- Plugins (vst2/vst3/clap) link and load-bearing (ROM present); "makes sound" host verification deferred
  to Phase 4 (needs interactive REAPER).

## 3b. Deviations / vendored-tree edits (lost on submodule checkout — re-apply after Phase 1)

These are **local build-environment** edits inside the `iPlug2` submodule working tree. They are NOT
committed (gitlink pointer is unchanged; root shows `m iPlug2` only because the junction makes the
tracked stub `VST3_SDK/README.md` differ in content). Re-create them after any submodule checkout.

1. **VST3 SDK junction**: `iPlug2/Dependencies/IPlug/VST3_SDK` is an NTFS junction to `D:\opt\vst\vst3sdk`
   (the v3.8.1 clone). The submodule normally tracks only a stub `README.md` there; `download-iplug-sdks.sh`
   / `download-vst3-sdk.sh` is the git-clean alternative (clones a pinned SDK). The vst3 vcxproj references
   SDK sources by literal relative path, so the path must be populated either way. Phase 3 CMake will resolve
   the SDK via `SW10_VST3_SDK_DIR` cache var instead of relying on this junction (AGENTS.md hard rule 3).
2. **Prebuilt libs** at `iPlug2/Dependencies/Build/win/...` + `Build/src/...`: produced by
   `download-prebuilt-libs.sh win`; gitignored, regenerate as needed.
3. `SW10_PLUG/SW10_PLUG.cpp` `handleDllPath` `…A` API fix (in the main repo, persists — see Phase 0 notes).
4. **Non-admin builds**: postbuild copies to `C:\Program Files\…\VstPlugins`/VST3/CLAP fail with Access denied.
   Override per-invocation (props use `Condition="'$(…)'==''"`) to a writable staging dir, e.g.
   `/p:VST2_32_PATH=…\build-win\deploy\vst2-32 /p:VST2_64_PATH=… /p:VST3_32_PATH=… /p:VST3_64_PATH=… /p:CLAP_PATH=…`
   (dirs must pre-exist; `copy` fails otherwise). Caveat: an up-to-date-skipped project does not re-run its
   postbuild, so deploy staging can miss binaries — copy manually or use `Rebuild`. Verified 2026-09-15: all four
    targets Release x64+Win32 build clean (toolset override `v145`); app smoke OK (window+ROM). ROM copied
    manually next to each output (incl. vst3 bundle `Contents\x86*-win`); automation stays a Phase 3 item.

Phase 1 additions (2026-09-15, pin `d54f69050`):

5. **vcxproj edits (main repo, persist)**: added `<ClCompile Include="..\..\iPlug2\WDL\win32_utf8.c" />` to all four
   live vcxprojs (`app`, `vst2`, `vst3`, `clap`). The new pin's `IPlugPlatform.h` routes Win32 file/dialog/profile APIs
   through WDL's `*_UTF8` wrappers (`win32_utf8.h`), whose definitions live in `WDL/win32_utf8.c`; upstream CMake
   (`Scripts/cmake/IPlug.cmake`) compiles it — the legacy vcxprojs must too (was: 11 LNK2001 at app link). No iPlug2
   tree edits were required anywhere in Phase 1 — the submodule working tree is untouched apart from the junction (item 1).
6. **`SW10-win.props` legacy-macro shim** (`SW10_PLUG/config/SW10_PLUG-win.props`, main repo): after the props import,
   a `LegacyMacros` PropertyGroup re-defines `VST2_32/64_PATH`, `VST3_32/64_PATH`, `AAX_32/64_PATH`, `CLAP_PATH`,
   and `*_HOST_PATH` with `Condition="'$(…)'==''"` — upstream renamed these to `_X64_`/`_ARM64EC_`, but our
   `postbuild-win.bat` invocation still consumes the old names; conditions keep the `/p:` deploy redirects working.
7. **Deps regenerated, not hand-patched** (all gitignored inside the submodule): prebuilt libs re-downloaded
   (`skunicode` → `skunicode_core`+`skunicode_icu` set), CLAP SDK refreshed to 1.2.10, VST3 junction re-created
   (`cmd /c rmdir` on junction → `git checkout -- .` restored stub → `mklink /J` again; `git -C iPlug2 status`
   shows only the expected `M Dependencies/IPlug/VST3_SDK/README.md`). VST2 headers remain untracked copies in the
   submodule stub dir (see §1 SDK table).
8. **Upstream CMake found** (new pin: `iPlug2/CMakeLists.txt`, `iPlug2.cmake`, `Scripts/cmake/FindiPlug2.cmake`,
   per-API `.cmake` with Win32 source lists, `CMakePresets.json`). Phase 3 should evaluate consuming iPlug2 via
   `find_package(iPlug2)` / `add_subdirectory` instead of hand-writing `sources_core.cmake` — likely a big scope cut,
   but verify /MT, `_xp`-era flags and our deploy hooks are expressible before switching.

## 4. Phase 1 — iPlug2 update (DONE 2026-09-15)

- [x] `git -C iPlug2 fetch origin && git checkout <tip>` → **`d54f69050f517e43b941d88c2a170f0a840b9ee4`** (2026-08-19,
      "Merge PR #1406 cmake/fix-surround-effect"; 944 commits from `6c639b97f`). Detached HEAD as before.
      Submodule bump committed at root as dedicated commit **`aa9dad9`** (gitlink only). No `iPlug2/.gitmodules`
      in the new pin (WDL etc. are vendored, not submodules — nothing to `submodule update`).
      Note: fetch showed refname conflicts for `cmake/*`, `clap/additions` (old flat branches `cmake`/`clap`
      shadow them) — harmless, `master` fetched fine; `git remote prune origin` would clean it.
- [x] Dependency re-downloads (network OK):
      - `download-prebuilt-libs.sh win` re-run → `Build/win/{Win32,x64}/{Debug,Release}` now the `v1.0.0-beta`
        package; lib set changed: `skunicode` → **`skunicode_core` + `skunicode_icu`** (+skottie/skparagraph/sksg,
        freetype, libpng, zlib, svg) — matches new-master `IGraphicsSkia.cpp` pragmas. `Build/src/{skia,freetype}`
        headers regenerated.
      - `download-clap-sdks.sh` → CLAP_SDK 1.2.1→1.2.10 + clap-helpers main (`plugin.hxx`/`plugin.hh` present).
      - VST3: kept the junction `iPlug2/Dependencies/IPlug/VST3_SDK` → `D:\opt\vst\vst3sdk`; new
        `download-vst3-sdk.sh` clones exactly that layout (vst3sdk + base/pluginterfaces/public.sdk submodules),
        so the junction remains valid. WAM not needed (no WAM CMake target).
- [x] Merge upstream `common-win.props`: upstream props **moved** to iPlug2 root (`iPlug2/common-win.props`) and is
      the file actually imported by `SW10-win.props` → the submodule copy is the live one; upstream gained
      CLAP macros, `_X64_`/`_ARM64EC_` deploy/host macros, `ICUDAT_PATH`, `REAPER_SDK` path fix, warnings
      `26451;26812`; dropped FAUST/IMGUI macros. Root `common-win.props` (orphan — nothing imports it, kept as
      reference) re-synced to the new upstream + re-added local bits: `IPLUG2_ROOT`, legacy `_32_/_64_` deploy +
      REAPER host macros, `COPY_VST2=1` (still upstream too). Upstream `Examples/config/*` is gone; drift against
      `SW10_PLUG-win.props` handled there (see §3b item 5).
- [x] Fix API drift in plugin sources — full diff-audit + build iteration:
      - `IPlugReaperVST2.h` deleted upstream: **not referenced** by any SW10 source/vcxproj → no action.
      - New `IPlugPlatform.h` pulls in WDL's UTF-8 API (`fopenUTF8`, `MessageBoxUTF8`, `…ProfileStringUTF8`, …) →
        added `iPlug2/WDL/win32_utf8.c` ClCompile to all four vcxprojs (mirrors upstream `Scripts/cmake/IPlug.cmake`).
        This was the ONLY build break (app link, then fixed before others linked).
      - `IPlugVST3_View.h`/CLAP glue/IGraphics changes: no SW10-visible signature breakage; `IVLEDMeterControl<N>`,
        `IVKeyboardControl` usage compiles unchanged. No iPlug2 tree edits were needed.
- [x] Rebuild sln fallback post-update: **fallback ok** — all four targets (app/vst2/vst3/clap) ×
      Release × {x64, Win32} build clean (`/p:PlatformToolset=v145` + deploy-path redirects, §3b item 4);
      AAX untouched/unbuilt (no SDK, dropped per §2.3). App smoke test passes (window "SW10_PLUG", ROM loaded).

### Phase 1 build matrix (2026-09-15, pin `d54f69050`)

| Target | x64 Release | Win32 Release |
|---|---|---|
| app (exe) | ✅ | ✅ |
| vst2 (dll) | ✅ | ✅ |
| vst3 | ✅ | ✅ |
| clap | ✅ | ✅ |

Smoke: `SW10_PLUG.exe` (x64) alive after 5s, MainWindowTitle `SW10_PLUG` ✅.

## 5. Phase 2 — SDK wiring (design for CMake cache variables) — **DONE 2026-09-15**

| CMake var | Default | Points at |
|---|---|---|
| `SW10_VST3_SDK_DIR` | `$ENV{VST3_SDK_DIR}` → fallback `iPlug2/Dependencies/IPlug/VST3_SDK` | `D:/opt/vst/vst3sdk` (set via env or preset) |
| `SW10_VST2_SDK_DIR` | `$ENV{VST2_SDK_DIR}` | `D:/opt/vst/vstsdk2.4` |
| `SW10_CLAP_DIR` | `iPlug2/Dependencies/IPlug` | contains `CLAP_SDK`, `CLAP_HELPERS` |
| `SW10_DEPS_WIN_DIR` | `iPlug2/Dependencies/Build/win` | prebuilt Skia/Freetype libs (run download script first) |
| `SW10_ROM_PATH` | `SW10_PLUG/build-win/ROMSXGM.BIN` | copied to every output dir post-build |

Presets store real absolute paths for the local machine; CI overrides with workspace paths / artifacts.

### Phase 2 done (2026-09-15) — notes

- `cmake/iplug2_paths.cmake` implements the table above as `SW10_*` cache vars with env fallbacks
  (`VST3_SDK_DIR`, `VST2_SDK_DIR`) and FATAL_ERROR messages naming the exact fix. Consumed from the new
  root `CMakeLists.txt` (min 3.25). `cmake -B b32 -A Win32` and `-A x64` both configure with SDK
  detection green (VS 18 2026 generator; no CMake-4 policy hacks needed — we do **not** add_subdirectory
  vst3sdk, upstream `VST3.cmake` compiles the needed SDK sources directly).
- Upstream consumption route: `include(iPlug2/iPlug2.cmake)` + `find_package(iPlug2 REQUIRED)` exactly like
  upstream Examples — **not** `add_subdirectory(iPlug2)` (that would drag in Examples/Tests). `SW10_IPLUG2_DIR`
  forwards into upstream's `IPLUG2_DIR`. Upstream has no SDK-path cache vars (paths are hardcoded
  `IPLUG2_DIR`-relative), so `iplug2_paths.cmake` maps the SW10 knobs *onto* those locations:
  auto-creates the `Dependencies/IPlug/VST3_SDK` junction (mklink /J, no admin) pointing at
  `SW10_VST3_SDK_DIR`, and copies `aeffect.h`/`aeffectx.h` into the `VST2_SDK` stub from
  `SW10_VST2_SDK_DIR` when missing (automates the Phase-1 manual step; untracked/gitignored files only,
  gitlink untouched).
- `SW10_DEPS_WIN_DIR` is only hard-checked when `IGRAPHICS_BACKEND=SKIA` (NanoVG needs no downloads).
  `SW10_COPY_ROM` (ON by default) FATALs on a missing ROM locally; ci presets set it OFF.

## 6. Phase 3 — CMake build system (core work) — **DONE 2026-09-15**

### Phase 3 results (VS 18 2026, NanoVG/GL2)

Full matrix green first pass: `cmake --preset vs-{x64,win32}` + `cmake --build --preset vs-*-release` →
8/8 targets link with **zero errors/warnings introduced** (only the two pre-existing source warnings
C4477/C4700). Artifacts exactly mirror sln naming/layout:

| Target | x64 | Win32 |
|---|---|---|
| `build-cmake/app/<arch>/Release/SW10_PLUG.exe` | ✅ smoke (window `SW10_PLUG`, ROM) | ✅ smoke |
| `build-cmake/vst2/<arch>/Release/SW10_PLUG.dll` | ✅ | ✅ |
| `build-cmake/vst3/<arch>/Release/SW10_PLUG.vst3/Contents/{x86_64-win,x86-win}/SW10_PLUG.vst3` (+`Resources/`) | ✅ | ✅ |
| `build-cmake/clap/<arch>/Release/SW10_PLUG.clap` | ✅ | ✅ |

`ROMSXGM.BIN` staged post-build next to every binary incl. the vst3 bundle bin dir (`SW10_COPY_ROM`,
ci presets set OFF). `/MT` verified via `dumpbin /dependents` (no `vcruntime140.dll`/`msvcp140.dll`;
`OPENGL32.dll` present via the NanoVG pragma). No `.def` files needed (entries are `dllexport`ed).

Deviations from the §6 sketch (all deliberate):

- **Graphics backend = NanoVG/GL2, not Skia.** Plan §6's "Skia for CMake" predates discovering that
  upstream's *default* Windows backend is NanoVG/GL2 (matches the proven sln artifacts; `IGRAPHICS_BACKEND`
  defaults to `NANOVG`) and that upstream's Windows `iPlug2::IGraphics::Skia` target links a
  `Build/src/skia/out/Release-x64` merged-lib layout our `download-prebuilt-libs.sh` deps do not ship.
  `SW10_DEPS_WIN_DIR` (FAT-checked only when `IGRAPHICS_BACKEND=SKIA`) keeps the Skia door open; a
  working build beat an ideal one (task brief).
- **No `sources_core.cmake`/`rt_audio_midi.cmake`/`cmake/vst2|vst3|clap` glue** — upstream
  `iPlug2::{IPlug,IGraphics,APP,VST2,VST3,CLAP,Extras::Synth}` INTERFACE targets (consumed via
  `include(iPlug2/iPlug2.cmake)` + `find_package(iPlug2)`) ARE the source lists; `SW10_PLUG/CMakeLists.txt`
  only creates the 4 targets (`sw10_app`, `sw10_vst2`, `sw10_vst3`, `sw10_clap`), calls
  `iplug_configure_target()` per API, then overrides output dirs to `build-cmake/<api>/<arch>/<Config>/`.
- vst3sdk is **not** `add_subdirectory`'d — upstream `VST3.cmake` compiles the needed SDK sources into
  the plugin target directly, so `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded…` covers them trivially
  (the §1 "vst3sdk /MD default" risk never materialized; no CMake-4 policy override needed either).
- `resources/main.rc` compiled on **all four** targets (sln parity for version-info/dialog IDs; APP-only
  functionally). Upstream's `iplug_configure_vst3` POST_BUILD still `make_directory`s a stray
  `build-cmake/_bld/*/out/SW10_PLUG.vst3/Contents/Resources` because we redirect outputs — cosmetic.
- `IPLUG_DEPLOY_PLUGINS` forced OFF; `SW10_POSTBUILD_INSTALL=ON` implements the old Program-Files
  deploy ourselves (VST2→`%PROGRAMFILES%\VstPlugins`, VST3→`%CommonProgramFiles%\VST3`,
  CLAP→`%CommonProgramFiles%\CLAP`) since upstream's deploy paths assume its `out/` layout.
- **XP decision gate (§2.4): FAILED on this box** — `vs-win32-xp`/`vs-xp64` presets exist but configure
  errors `MSB8020: build tools for v141_xp cannot be found` (VS 2026 registers toolsets v150–v180 only;
  the v141 14.16 compiler is present but not the `_xp` toolset wiring / 7.1A SDK). Presets kept and
  documented; XP shipping needs the "MSVC v141, VS 2017 v141_xp" VS component or the clang-cl Plan B.
  NanoVG backend removes the prebuilt-Skia XP blocker; **Win7 x86 remains the practical floor**
  (static CRT on modern toolset runs on Win7).

### New files

> Implemented: `CMakeLists.txt`, `CMakePresets.json`, `cmake/iplug2_paths.cmake`,
> `SW10_PLUG/CMakeLists.txt`. The `sources_core.cmake`/`rt_audio_midi.cmake`/per-API glue below were
> superseded by upstream's CMake modules (see deviations above); sketch kept for reference.

```
CMakeLists.txt              # top-level; options SW10_BUILD_{APP,VST2,VST3,CLAP} (all ON)
CMakePresets.json           # configure/build presets, see below
cmake/
  iplug2_paths.cmake        # Phase 2 cache vars, SDK existence checks (clear errors)
  sw10_common.cmake         # sw10_core lib: defines, includes, warning suppressions, /MT, ROM post-build
  sources_core.cmake        # API-independent source lists
  rt_audio_midi.cmake       # RTAudio/RTMidi/ASIO lib (app only)
cmake/vst2/cmake/clap/      # per-API glue: entry wrappers if needed, .def files, target suffixes
```

### Target graph

```
sw10_core (STATIC, $<CONFIG>/$<ARCH>-aware defines)
  ├─ plugin sources: SW10_PLUG.cpp, VLSG.cpp, config.h include dir
  ├─ IPlug: IPlugAPIBase/IPlugParameter/IPlugPaths/IPlugPluginBase/IPlugProcessor/IPlugTimer
  ├─ IGraphics: IGraphics, IControl, IGraphicsEditorDelegate, Controls/{IControls,IPopupMenuControl,ITextEntryControl},
  │             Drawing/IGraphicsSkia.cpp, Platforms/IGraphicsWin.cpp  (NO NanoVG — Skia backend)
  ├─ WDL: compiled from source (iPlug2/WDL + WDL/IPlug + WDL/lice subsets — start from the vcxproj
  │       ClCompile lists; do NOT try to reuse the vcxproj's prebuilt wdl static libs)
  ├─ defines: WIN32, NDEBUG/_DEBUG, NOMINMAX, _CRT_SECURE_NO_(NONSTDC_)DEPRECATE, IPLUG_EDITOR=1, IPLUG_DSP=1
  │           + per-API: VST2_API/VST3_API/CLAP_API/APP_API and APP's __WINDOWS_DS__/__WINDOWS_MM__/__WINDOWS_ASIO__
  └─ per-API wrapper (SHARED/MODULE; each sets only its API define so IPlug_include_in_plug_src.h emits right entry):
       sw10_app  (exe)    + IPlug/APP/*.cpp + RTAudio/RTMidi, links winmm,dsound
       sw10_vst2 (dll)    + IPlug/VST2/IPlugVST2.cpp (+ vst2 .def exporting Main; confirm exact needs against new master)
       sw10_vst3 (module) + IPlug/VST3/IPlugVST3*.cpp + vst3sdk via add_subdirectory(${SW10_VST3_SDK_DIR})
                           (options: examples OFF, VSTGUI OFF unless needed) linking sdk/base/pluginterfaces subtargets
                           OR compile the SDK source subset directly if add_subdirectory fights us; SUFFIX .vst3
       sw10_clap (dll)    + IPlug/CLAP/IPlugCLAP.cpp + clap-helpers entry (`clap-helpers/src/library/clap_clap_plugin.cpp`
                           — confirm from clap vcxproj during impl); SUFFIX .clap
```

### Requirements / gotchas

- Multi-config VS generator only (`-A Win32|x64`); do not promise Make/Ninja for Win32 XP presets.
- Per-config link dir `SW10_DEPS_WIN_DIR/$<PLATFORM>/$<CONFIG>` for the pragma-commented Skia libs (`LINK_DIRECTORIES`/`target_link_directories`). `skunicode`+ICU subset: confirm lib set against `download-prebuilt-libs.sh` output for Win32 AND x64.
- `/MT` (MultiThreaded, not DLL) runtime — matches `common-win.props`; keep it or document why DLL CRT is now required by vst3sdk (likely NOT, but vst3sdk defaults /MD — force /MT for consistency with the old deliverables and XP portability).
- `/bigobj` may be needed for Skia TUs (vcxproj: check `LargeObjects`).
- Output layout: `build-cmake/<api>/<arch>/$<CONFIG>/` e.g. `build-cmake/vst2/Win32/Release/SW10_PLUG_VST2.dll` — mirror existing naming (`SW10_PLUG_Win32.exe` etc.) for continuity.
- Post-build: copy `SW10_ROM_PATH` next to every produced binary (hard runtime requirement, `handleDllPath`). Optional `SW10_POSTBUILD_INSTALL=ON` mirror of old postbuild step (copy VST2→`%ProgramFiles%\VstPlugins`, VST3→`%CommonProgramFiles%\VST3`, CLAP→`%CommonProgramFiles%\CLAP`) — OFF by default in CI.
- WindowsVersion: for Win7 floor set `/D_WIN32_WINNT=0x0601` etc.
- **XP presets**: try VS2026 "C++ build tools for v141_xp / Windows 7.1A SDK" components via `-T host=x86,v141_xp` (probably absent for VS2026) → Plan B: clang-cl + legacy SDK/crt dirs in `CMAKE_..._INIT`. **Decision gate**: test Skia target on XP VM; if Skia can't run, XP builds = VST2 only (document).
- `CMakePresets.json` v3+: `base` preset (VS 18 2026 generator, x64), `vs-win32`, `vs-win32-xp`, CI preset using env SDK paths.

## 6b. MinGW-w64 (MSYS2) toolchain support — **DONE 2026-09-15**

The CMake build now also targets **MinGW-w64 (GCC and Clang) via the Ninja generator** in
addition to MSVC/Visual Studio. Verified in the MSYS2 MINGW64 shell (GCC 16.2, Clang 22.1,
CMake 4.x): all four targets (app/vst2/vst3/clap) build under BOTH compilers, are
self-contained (system-DLL imports only, no `libgcc`/`libstdc++`/`winpthread`), export the
correct entry points (`VSTPluginMain` / `GetPluginFactory` / `clap_entry`), and pass the
DAW-less `loadtest.ps1` (real Win64 `LoadLibrary` + init + factory enumeration).

**Build (MSYS2 MINGW64 shell):**
```sh
cmake --preset mingw-x64         && cmake --build --preset mingw-x64-release   # GCC
cmake --preset mingw-clang-x64   && cmake --build --preset mingw-clang-x64-release
pwsh SW10_PLUG/scripts/loadtest.ps1 -Arch x64 -ArchDir x64-mingw        # GCC   (added -ArchDir)
pwsh SW10_PLUG/scripts/loadtest.ps1 -Arch x64 -ArchDir x64-mingw-clang  # Clang
```
Output lives in `build-cmake/<api>/x64-mingw[-clang]/<Config>/` (the `-clang` suffix keeps the
two toolchains from clobbering each other; `SW10_ARCH` carries the suffix, the VST3 bundle arch
still keys off `CMAKE_SIZEOF_VOID_P`). x86_64 only — a 32-bit MinGW build needs the separate
`mingw-w64-i686` toolchain and is not wired.

**Every change is guarded by `MINGW` / `Clang` / `__GNUC__` so the MSVC path is byte-identical.**
New files: `cmake/mingw_compat.cmake` (auto-included at configure when `MINGW`),
`cmake/mingw_portability_prelude.h` (force-included into every MinGW C++ TU). No iPlug2
submodule edits were required — everything lives in the superproject, so nothing is lost on a
submodule checkout (contrast §3b).

What MinGW needs that MSVC gives for free:
- **Root gate** widened from `NOT MSVC` to `NOT (MSVC OR MINGW)`; `CMAKE_MSVC_RUNTIME_LIBRARY`
  now MSVC-only (`mingw_compat` handles the MinGW static runtime instead).
- **MSVC `.lib` names** hard-coded on the upstream `iPlug2::IPlug`/`iPlug2::APP`/`OSC`
  INTERFACE targets (`Shlwapi.lib`, `comctl32.lib`, `wininet.lib`, `dsound.lib`, `winmm.lib`,
  `ws2_32.lib`) are rewritten to bare `-l` names in `mingw_compat` (ld won't find `Foo.lib`;
  Skia/WebView2 file-path libs are left alone — unused by the NanoVG MinGW path).
- **NanoVG/GL2 is the enabling choice**: it compiles `nanovg.c`+`glad.c` from source inside
  `IGraphicsWin.cpp` and `glad` `LoadLibrary`s `opengl32.dll`, so NO MSVC-built prebuilt graphics
  libs are linked (unlike SKIA). `wgl*`/`SwapBuffers` are still referenced directly, so
  `mingw_compat` `link_libraries(opengl32 gdi32)` (GCC ignores `#pragma comment(lib,...)`).
- **Compile/link flags**: `-Wa,-mbig-obj` (the IGraphicsWin unity TU overflows COFF section
  limits), `-fpermissive` (GCC: iPlug/APP assign `FARPROC`→`void*`), static runtime
  (`-static -static-libgcc -static-libstdc++`), `_USE_MATH_DEFINES` (M_PI under `__STRICT_ANSI__`).
- **Prelude force-include** fixes libstdc++ missing transitively-included headers
  (`<memory>`→`std::unique_ptr` in `VoiceAllocator.h`, `<cmath>`, `<ctime>`, …) across iPlug/WDL/SDK.
- **Clang extras**: `GetProcAddress` is macro-wrapped to cast to `void*` (clang hard-errors on the
  FARPROC→void* conversion that `-fpermissive` cannot downgrade) — defined only AFTER
  `<windows.h>` so the header's own declaration isn't clobbered; and `UNICODE`/`_UNICODE`
  per-source on Steinberg `fstring.cpp`/`dllmain.cpp` (they pass `wchar_t*` to `FoldString`/
  `GetModuleFileName`, which pick the ANSI entry points without `UNICODE`; nothing in the build
  sets it globally because the MSVC ANSI build only survives via MSVC leniency).
- **Two first-party source fixes**: `SW10_PLUG.h` `clock_gettime`/`i64` shim now `_MSC_VER`-only
  (MinGW gets a real POSIX `clock_gettime` from the prelude); `SW10_PLUG.cpp` locally expands
  `CLAP_EXPORT` to `extern __attribute__((dllexport))` for GNU/Clang around the CLAP entry
  definitions (GCC/Clang reject `dllexport` on a namespace-scope `const`; MSVC tolerates it).
- `loadtest.ps1` gained `-ArchDir` (defaults to `-Arch`) so it can target the MinGW output dirs.

## 7. Phase 4 — Verification matrix (definition of done) — **DONE 2026-09-15**

For each cell: build OK + binary loads & makes sound:

### Phase 4 verified (2026-09-15, CMake builds via `vs-*-release` presets, NanoVG/GL2)

| Target | Win32 | x64 |
|---|---|---|
| app (exe) | ✅ build + smoke (window `SW10_PLUG`, ROM) | ✅ build + smoke |
| vst2 (dll) | ✅ build + load-harness¹ | ✅ build + load-harness¹ |
| vst3 | ✅ build + load-harness¹ | ✅ build + load-harness¹ |
| clap | ✅ build + load-harness¹ | ✅ build + load-harness¹ |

¹ `SW10_PLUG/scripts/loadtest.ps1 -Arch <a>` (PowerShell P/Invoke; auto-relaunches 32-bit artifacts
under SysWOW64 PowerShell): LoadLibrary with ROM staged → VST2 `VSTPluginMain` → `AEffect` magic `VstP` /
unique `0x53573130` ('SW10') / version 3 + `effOpen`/`effGetVersion`/`effClose`; VST3 `InitDll` →
`GetPluginFactory` → `countClasses`=1 + `getClassInfo` ('SW10…', 'Audio Module Class') + Release +
`ExitDll` (note: on x86 `PLUGIN_API` is `__stdcall` → `GetPluginFactory` is exported `_GetPluginFactory@0`
and vtable calls need stdcall delegates — the harness handles both); CLAP `clap_entry` v1.2 →
`init(path)`=true → `get_factory("clap.plugin-factory")`² non-null → `deinit`.
² CLAP 1.2 factory id is `"clap.plugin-factory"` (`clap/factory/plugin-factory.h`), not the old `"clap.plugin"`.

**Sound: pending interactive host — REAPER is not installed on this box**, so no cell claims audio
output; load-harness pass = module loads, entry points respond, ROM found next to the binary.


> Interim (2026-09-15, MSBuild fallback on current pin): all four targets **build** in both archs and the
> **app** smoke-passes (window + ROM + UI) in Win32 & x64 — see Phase 0 baseline notes. The CMake matrix above
> (Phase 4) supersedes it; only *sound* in an interactive host remains open.

- Load hosts available: REAPER paths auto-detected in old props (`VST2_64_HOST_PATH`…) — reuse `sw10_* /DEBUG launch`
  presets for VS debugging per target (old vcxproj `LocalDebugger` settings embed REAPER + `SW10_PLUG.RPP`).
- Smoke test at minimum: `sw10_app` launches with UI (MIDI input device may be absent — at least window + knobs render), plugin binaries load in REAPER w/ ROM found.
- `git status` clean except intended files; no `build-cmake` committed.

## 8. Phase 5 — CI & scripts & docs — **DONE 2026-09-15**

- [x] Rewrote `.github/workflows/build-native.yml`: `PROJECT_NAME: SW10_PLUG`; matrix over `ci-win64`/`ci-win32`
      presets; steps = checkout (submodules) → checkout `vst3sdk@v3.8.1_build_84` (submodules) →
      `download-prebuilt-libs.sh` + `download-clap-sdks.sh` (Git-Bash) → `cmake --preset ci-win* -DSW10_BUILD_VST2=OFF`
      → `cmake --build --preset ci-win*-release` → app smoke → **VST2 gated OFF** with an inline comment on how to
      re-enable (private artifact/self-hosted runner + `VST2_SDK_DIR`). Artifacts per arch (`upload-artifact@v4`) +
      a `ROM-REQUIRED.txt` stub per output dir (ROM not distributable). `build-wam.yml` untouched; not pushed/run.
- [x] `makedist-win.bat`: **CMake is the default path** (prebuild `prepare_resources-win.py`/`update_installer_version.py`
      stay → `cmake --preset vs-x64/vs-win32` + build Release → stage `build-cmake/*` into legacy `build-win/` names so
      the unchanged `installer/SW10_PLUG.iss` works → InnoSetup `iscc` if present, else skip). Old msbuild flow kept
      behind `-Legacy` (still points at the VS2019 `vcvarsall`; a comment notes the VS2026 path + `/p:PlatformToolset=v145`).
      Batch not executed here (no InnoSetup/VS2019 on box); reviewed for cwd correctness (root vs `SW10_PLUG/`).
- [x] `README.md`: added a CMake quickstart (presets, output layout, `SW10_BUILD_*` toggles, NanoVG/GL2, `/MT`,
      ROM auto-stage, SDK env/cache vars, `loadtest.ps1`, `makedist-win.bat`), and the "Not included" list notes the
      ROM is a runtime requirement next to each binary.
- [x] `AGENTS.md`: build commands → real working presets + `loadtest.ps1` + `makedist-win.bat`; corrected the pin
      (`d54f69050`) and graphics (CMake = NanoVG/GL2, Skia caveated); repo-map now points at the CMake files/CI;
      Status section has dated Phase 2–5 lines with commit hashes and deviations.

- [x] **Repo discovery:** the root `.gitignore` `build-*` pattern was unanchored and silently ignored
      `.github/workflows/build-native.yml` **and** `build-wam.yml` — neither was ever tracked (the old
      native CI literally never ran on GitHub!). Fixed: `/build-*` + `SW10_PLUG/build-*` + `iPlug2/**/build-*`;
      `build-native.yml` (rewritten) now commits; `build-wam.yml` joins the index **content-unchanged**
      (only now can it be versioned). `SW10_PLUG/.gitignore` has its own `build-*` (kept; still covers
      `SW10_PLUG/build-win`), and `ROMSXGM.BIN`/`aeffect*.h` stay ignored repo-wide.

### Remaining open items (not Phase 5 blockers)
- **Sound / interactive-host (REAPER) verification** — deferred (no REAPER on the box); Phase 4 harness proves load
  + entry-point wiring only.
- **XP floor**: `-xp` presets present but inert until the `v141_xp` toolset + Win7.1A SDK are installed (or clang-cl
  Plan B). Win7 x86 is the pragmatic floor today (NanoVG/GL2, static CRT).
- **Skia backend**: still selectable but Windows upstream lib layout diverges from our deps zip; untested at link.
- Skia-lib `SW10_DEPS_WIN_DIR` wiring is present but only FAT-checked when `IGRAPHICS_BACKEND=SKIA`.

## 9. Risks

| Risk | Likelihood | Mitigation |
|---|---|---|
| iPlug2 585-commit jump breaks plugin/props/IGraphics usage | High | Phase 1 dedicated pass; sln fallback kept; compile early, iterate |
| Prebuilt Skia (new deps zip) lacks Win32 or no XP support | Medium | Test Win32 link early in Phase 3; XP = decision gate, VST2-only fallback |
| VS2026 can't install XP-era toolset | High | clang-cl Plan B; or declare Win7 floor; or keep an old VS2019/2022 build machine note |
| vst3sdk 3.8.1 subtarget linkage vs iPlug2 glue | Medium | Fall back to compiling required `base`+`public.sdk` sources into a static lib directly (list mirrors vst3vcxproj) |
| VST2 SDK path assumptions (IPlugVST2 needs full vstsdk2.4 tree?) | Medium | Point include dirs at `vstsdk2.4` root; confirm exact required sources from vcxproj during Phase 3 |
| CLAP 32-bit | Low | CLAP spec supports win32; old clap vcxproj had Win32 configs |
| WDL compiled directly vs prebuilt libs divergence | Medium | Start from vcxproj source lists verbatim; diff link errors |
| ROM path changes when moving to new output dirs | Low | Post-build copy enforced by CMake (`POST_BUILD` command), covered by smoke tests |

## 10. Appendix

- XP diff (to preserve): `WindowsTargetPlatformVersion 10.0→7.0`, Release|Win32 + Release|x64 `PlatformToolset v141_xp→ClangCL`.
- Old sln target list: app, vst2, vst3, aax, clap (Win32+x64 each), Debug/Release/Tracer configs.
- iPlug2 CMake branch exists but unmerged: `origin/claude/linux-headless-support-01KWkH3gdwHaY7eu4oWhpPUW` (`a15ce5d0c` "Merge cmake-2 branch") — borrow source lists/ideas from `git show a15ce5d0c` if useful, do not depend on it.
