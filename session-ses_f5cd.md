# Fix CI CLAP SDK download failure

**Session ID:** ses_f5cdcd3c4ffeq32idg6Q8oq0rX
**Created:** 9/15/2026, 1:36:26 PM
**Updated:** 9/15/2026, 1:37:48 PM

---

## User

GitHub CI appears to be failing for downloading CLAP SDKs step, please fix

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 15.8s)

_Thinking:_


The user is saying that the GitHub CI is failing at the "downloading CLAP SDKs" step. Let me check the CI workflow and the CLAP download script.







**Tool: bash**

**Input:**
```json
{
  "command": "git status --short && git log --oneline -5"
}
```

**Output:**
```
 m iPlug2
8165f7d Qwen3.8-flash-next-oQ5: CMake compatibility with MinGW and clang
f157e61 Phase 5: CI workflow, makedist CMake path, README/AGENTS/plan docs
f5136b5 Phase 4: load-test harness + verification matrix results
835fa04 Phase 3: CMake build system (app/vst2/vst3/clap, Win32+x64 Release)
98788a4 Phase 2: CMake SDK wiring (SW10_* cache vars, both archs configure)

```

**Tool: grep**

**Input:**
```json
{
  "pattern": "CLAP",
  "path": "D:\\Projects\\vst\\sw10_plug\\.github\\workflows"
}
```

**Output:**
```
Found 1 matches
D:\Projects\vst\sw10_plug\.github\workflows\build-native.yml:
  Line 48:     - name: Download CLAP SDK + helpers

```

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 5.9s)

_Thinking:_


Let me read the workflow file, especially the CLAP download step.






**Tool: read**

**Input:**
```json
{
  "filePath": "D:\\Projects\\vst\\sw10_plug\\.github\\workflows\\build-native.yml"
}
```

**Output:**
```
<path>D:\Projects\vst\sw10_plug\.github\workflows\build-native.yml</path>
<type>file</type>
<content>
1: name: Build Native
2: 
3: on:
4:   push:
5:   workflow_dispatch:
6: 
7: env:
8:   PROJECT_NAME: SW10_PLUG
9: 
10: # CMake pipeline (Phase 5 of the build refactor — see REFACTOR_PLAN.md §8).
11: # The legacy msbuild/xcodeproj CI flow is retired here; the .sln remains the local
12: # fallback (msbuild needs /p:PlatformToolset=v145 on the current pin) and macOS/iOS
13: # continue to build via their xcodeproj. AAX is dropped (§2.3). build-wam.yml untouched.
14: jobs:
15:   build-windows:
16:     name: Windows CMake (${{ matrix.arch }})
17:     runs-on: windows-latest
18:     strategy:
19:       fail-fast: false
20:       matrix:
21:         include:
22:           - arch: x64
23:             preset: ci-win64
24:           - arch: Win32
25:             preset: ci-win32
26: 
27:     steps:
28:     - name: Check out repository (with iPlug2 submodule)
29:       uses: actions/checkout@v4
30:       with:
31:         submodules: true
32:         fetch-depth: 1
33: 
34:     - name: Check out VST3 SDK (v3.8.1_build_84, submodules)
35:       uses: actions/checkout@v4
36:       with:
37:         repository: steinbergmedia/vst3sdk
38:         ref: v3.8.1_build_84
39:         submodules: true
40:         path: vendor/vst3sdk
41:         fetch-depth: 1
42: 
43:     - name: Download iPlug2 prebuilt libs (Skia/Freetype — only needed for the SKIA backend)
44:       working-directory: iPlug2/Dependencies
45:       run: ./download-prebuilt-libs.sh win
46:       shell: bash
47: 
48:     - name: Download CLAP SDK + helpers
49:       working-directory: iPlug2/Dependencies
50:       run: ./download-clap-sdks.sh
51:       shell: bash
52: 
53:     - name: Configure (${{ matrix.arch }})
54:       # VST2 is deliberately OFF on CI: vstsdk2.4 is proprietary and cannot be
55:       # downloaded here. To build VST2, host the SDK as a private action artifact /
56:       # self-hosted runner and export VST2_SDK_DIR before this step (plus drop
57:       # -DSW10_BUILD_VST2=OFF), or use the local makedist-win.bat path.
58:       env:
59:         VST3_SDK_DIR: ${{ github.workspace }}/vendor/vst3sdk
60:       run: cmake --preset ${{ matrix.preset }} -DSW10_BUILD_VST2=OFF
61:       shell: pwsh
62: 
63:     - name: Build Release (${{ matrix.arch }})
64:       run: cmake --build --preset ${{ matrix.preset }}-release
65:       shell: pwsh
66: 
67:     - name: "Smoke: app launches"
68:       run: |
69:         $exe = "build-cmake/app/${{ matrix.arch }}/Release/SW10_PLUG.exe"
70:         $p = Start-Process $exe -PassThru
71:         Start-Sleep -Seconds 5
72:         $alive = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
73:         if ($null -eq $alive) { throw "app did not stay alive" }
74:         Stop-Process -Id $p.Id -Force
75:       shell: pwsh
76: 
77:     - name: Stage ROM for artifacts (path only — ROM file itself is not distributable here)
78:       run: |
79:         # Consumers must place ROMSXGM.BIN next to the binary (handleDllPath).
80:         # CI cannot ship it (copyrighted); drop a README stub into each output dir.
81:         Get-ChildItem build-cmake -Recurse -Directory -Filter Release | ForEach-Object {
82:           Set-Content -Path (Join-Path $_.FullName "ROM-REQUIRED.txt") -Value "Place ROMSXGM.BIN next to the plugin/app binary before use."
83:         }
84:       shell: pwsh
85: 
86:     - name: Upload artifacts (${{ matrix.arch }})
87:       uses: actions/upload-artifact@v4
88:       with:
89:         name: ${{ env.PROJECT_NAME }}-win-${{ matrix.arch }}
90:         path: |
91:           build-cmake/app/${{ matrix.arch }}/Release/
92:           build-cmake/vst2/${{ matrix.arch }}/Release/
93:           build-cmake/vst3/${{ matrix.arch }}/Release/
94:           build-cmake/clap/${{ matrix.arch }}/Release/
95:         if-no-files-found: error

(End of file - total 95 lines)
</content>
```

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 9.7s)

_Thinking:_


Let's check the actual CI failure. Let's check gh run log. But first, let's look at the download-clap-sdks.sh script in iPlug2/Dependencies.







**Tool: bash**

**Input:**
```json
{
  "command": "gh run list --limit 5 2>&1 | Select-Object -First 10"
}
```

**Output:**
```
[31;1mgh: [31;1mThe term 'gh' is not recognized as a name of a cmdlet, function, script file, or executable program.[0m
[31;1m[31;1mCheck the spelling of the name, or if a path was included, verify that the path is correct and try again.[0m

```

**Tool: bash**

**Input:**
```json
{
  "command": "Get-ChildItem \"iPlug2/Dependencies\" -Filter \"download-clap*\" | Select-Object Name"
}
```

**Output:**
```
(no output)
```

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 7.0s)

_Thinking:_


The `download-clap-sdks.sh` script doesn't exist? Let me check the contents of iPlug2/Dependencies. AGENTS.md said "CLAP: `iPlug2/Dependencies/IPlug/CLAP_SDK` + `CLAP_HELPERS` (there's a download script in that directory; run via Git-Bash)".






**Tool: bash**

**Input:**
```json
{
  "command": "Get-ChildItem \"iPlug2/Dependencies\" | Select-Object Name; Get-ChildItem \"iPlug2/Dependencies/IPlug\" -ErrorAction SilentlyContinue | Select-Object Name"
}
```

**Output:**
```

[32;1mName[0m
[32;1m----[0m
Build
Extras
IGraphics
IPlug
download-prebuilt-libs.sh
README.md
AAX_SDK
CLAP_HELPERS
CLAP_SDK
REAPER_SDK
RTAudio
RTMidi
VST2_SDK
VST3_SDK
WAM_AWP
WAM_SDK
download-clap-sdks.sh
download-iplug-sdks.sh
download-vst3-sdk.sh
download-wam-sdk.sh
README.md


```

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 9.5s)

_Thinking:_


The script is located at `iPlug2/Dependencies/IPlug/download-clap-sdks.sh`, not at `iPlug2/Dependencies/download-clap-sdks.sh`. In the workflow, it's set as `working-directory: iPlug2/Dependencies` and `run: ./download-clap-sdks.sh`, so it will fail with "No such file or directory". Fix: either change the working-directory to `iPlug2/Dependencies/IPlug`, or run `./IPlug/download-clap-sdks.sh`.

Also, check the contents of the script to confirm whether it works with bash on Windows CI, and verify whether the correct argument format is needed.

Found it — `download-clap-sdks.sh` is located in `iPlug2/Dependencies/IPlug/`, not in `iPlug2/Dependencies/`. Because of that, the CI step fails with "No such file or directory". Let me check whether the script actually works:






**Tool: read**

**Input:**
```json
{
  "filePath": "D:\\Projects\\vst\\sw10_plug\\iPlug2\\Dependencies\\IPlug\\download-clap-sdks.sh"
}
```

**Output:**
```
<path>D:\Projects\vst\sw10_plug\iPlug2\Dependencies\IPlug\download-clap-sdks.sh</path>
<type>file</type>
<content>
1: #!/usr/bin/env bash
2: set -eo pipefail
3: 
4: # 1st argument = tag name for CLAP SDK (e.g., "1.2.5")
5: # 2nd argument = tag name for CLAP helpers (optional, defaults to main)
6: 
7: CLAP_TAG="main"
8: HELPERS_TAG="main"
9: 
10: if [ "$1" != "" ]; then
11:   CLAP_TAG=$1
12: fi
13: 
14: if [ "$2" != "" ]; then
15:   HELPERS_TAG=$2
16: fi
17: 
18: rm -f -r CLAP_SDK
19: rm -f -r CLAP_HELPERS
20: 
21: git clone https://github.com/free-audio/clap.git --branch $CLAP_TAG --single-branch --depth=1 CLAP_SDK
22: git clone https://github.com/free-audio/clap-helpers.git --branch $HELPERS_TAG --single-branch --depth=1 CLAP_HELPERS
23: 
24: rm -rf CLAP_SDK/.git CLAP_HELPERS/.git
25: git checkout ./CLAP_SDK/readme.txt
26: git checkout ./CLAP_HELPERS/readme.txt

(End of file - total 26 lines)
</content>

<system-reminder>
Instructions from: D:\Projects\vst\sw10_plug\iPlug2\CLAUDE.md
# CLAUDE.md

This is the iPlug2 framework repository. Upstream: https://github.com/iPlug2/iPlug2.git

## Instructions

- Be concise. Assume the user knows audio programming concepts but not iPlug2 specifically.
- Check current directory before running scripts; return to repo root after.

## iPlug2 Overview

Cross-platform C++ audio plugin framework with two main components:

1. **IPlug** - Core plugin abstraction layer
2. **IGraphics** - GUI toolkit with multiple backends (NanoVG - lightweight, fast, Skia - heavyweight, better quality)

**Plugin Formats:** CLAP, VST3, AUv2/AUv3, AAX, WAM (VST2 is supported but deprecated)
**Extra Formats:** Standalone App, REAPER extensions (& REAPER plugins - plugins that call the REAPER API)
**Platforms:** macOS, iOS, Windows, Web (WAM/WASM)

## Repo Structure

```
├── IPlug/              # Core plugin abstraction
├── IGraphics/          # UI framework
├── IGraphics/Controls  # UI controls library
├── IGraphics/Platforms # Platform-specific implementations
├── IGraphics/Drawing   # Drawing implementations (NanoVG, Skia)
├── Dependencies/       # Third-party libs/SDKs (don't modify)
├── Examples/           # Template projects
├── Tests/              # Framework test projects
├── WDL/                # Cockos WDL (don't modify)
├── Scripts/            # Build utilities
├── Scripts/ci/         # CI/CD scripts
└── Documentation/      # Docs and wiki
```

## Key Concepts

- Three main files per project: `Plugin.cpp`, `Plugin.h`, `config.h`
- `ProcessBlock` must be realtime-safe (no allocations, locks, file I/O)
- Parameters: fixed count at compile time, indexed by enum, non-normalized values
- Use `GetParam(kIndex)->Value()` for parameter access
- IGraphics UIs typically built in a lambda in the plugin constructor
- IControls link to parameters via parameter ID
- Visualization controls can use an [ISender](@IPlug/ISender.h) to send data from audio thread to UI thread
- Controls can be grouped and tagged when Attached

- IGraphics is only one UI option; other options include SwiftUI, WebView. See examples in [Examples/](@Examples/).
- The default IGraphics backend NanoVG doesn't need the dependencies to be downloaded

## Code Style

- 2-space indentation, no tabs
- Unix line endings (except Windows-specific files)
- Member variables: `mCamelCase`
- Pointer args: `pCamelCase`
- Internal methods: `_methodName`
- Use C++17 (`override`, `final`, `auto`, `std::optional`, `std::string_view`)
- Avoid STL in core code; prefer WDL alternatives

## Resources

- [API Docs](https://iplug2.github.io/docs) | [Wiki](https://github.com/iPlug2/iPlug2/wiki)
- When building standalone app targets only debug builds have the debug menu with screenshot capabilities
</system-reminder>
```

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 13.3s)

_Thinking:_


The script is executed from within the `iPlug2/Dependencies/IPlug` directory (it does `git checkout ./CLAP_SDK/readme.txt` — a git checkout against the iPlug2 submodule's repository). The fix for the workflow: either set working-directory to `iPlug2/Dependencies/IPlug`, or invoke it via `./IPlug/download-clap-sdks.sh`. Note that this script runs `git checkout ./CLAP_SDK/readme.txt` — but wait, in CI the CLAP_SDK directory is untracked. The readme.txt file — does it exist in iPlug2's git index? Actually, `git checkout ./CLAP_SDK/readme.txt` restores the tracked readme.txt file that got deleted by `rm -rf`. Let's check whether the readme.txt file is tracked in iPlug2. If it's not tracked, `git checkout` will fail due to set -e. The local Phase 1 worked via Git-Bash, so presumably the readme is tracked. Let's check.

Also note that the script defaults to the "main" branch tag. No problem.

Let's check the git ls-files inside iPlug2.

The script needs to be run from `iPlug2/Dependencies/IPlug` (it performs relative clone there and uses `git checkout` to restore the tracked readme). Verify that the readme is being tracked, then fix the workflow's working-directory:






**Tool: bash**

**Input:**
```json
{
  "command": "git -C iPlug2 ls-files Dependencies/IPlug | Select-String readme"
}
```

**Output:**
```

Dependencies/IPlug/AAX_SDK/[7mREADME[0m.md
Dependencies/IPlug/CLAP_HELPERS/[7mreadme[0m.txt
Dependencies/IPlug/CLAP_SDK/[7mreadme[0m.txt
Dependencies/IPlug/[7mREADME[0m.md
Dependencies/IPlug/RTAudio/[7mREADME[0m.md
Dependencies/IPlug/RTAudio/contrib/python/pyrtaudio/[7mreadme[0m
Dependencies/IPlug/RTMidi/[7mREADME[0m.md
Dependencies/IPlug/RTMidi/msw/[7mreadme[0m
Dependencies/IPlug/VST2_SDK/[7mREADME[0m.md
Dependencies/IPlug/VST3_SDK/[7mREADME[0m.md
Dependencies/IPlug/WAM_AWP/[7mreadme[0m.txt
Dependencies/IPlug/WAM_SDK/[7mreadme[0m.txt


```

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 10.1s)

_Thinking:_


Since Readme is being tracked, if we run it from within the `Dependencies/IPlug` directory, the script will work (the git repository root is iPlug2, but `git checkout ./CLAP_SDK/readme.txt` uses a relative path from cwd so it's fine. Because cwd is inside the repository).

Fix: In the workflow, change the working-directory to `iPlug2/Dependencies/IPlug`. Also, should the prebuild libraries step be changed? No — `download-prebuilt-libs.sh` is in `iPlug2/Dependencies/`, and it's correct.

Furthermore, is the CLAP download even necessary in the first place? The CI builds the clap target, so yes, it's necessary.




The script is fine — the workflow just invokes it from the wrong directory. Fixing:



**Tool: edit**

**Input:**
```json
{
  "filePath": "D:\\Projects\\vst\\sw10_plug\\.github\\workflows\\build-native.yml",
  "oldString": "    - name: Download CLAP SDK + helpers\n      working-directory: iPlug2/Dependencies\n      run: ./download-clap-sdks.sh\n      shell: bash",
  "newString": "    - name: Download CLAP SDK + helpers\n      working-directory: iPlug2/Dependencies/IPlug\n      run: ./download-clap-sdks.sh\n      shell: bash"
}
```

**Output:**
```
Edit applied successfully.
```

---

## Assistant (Build · Qwen3.8-Flash-Next-oQ5e-mtp (oMLX) · 11.2s)

_Thinking:_


Quickly verify the script locally? Network + git are available. Can run it locally via git-bash to check if it works from that directory, but the local machine already has CLAP_SDK/CLAP_HELPERS (shown in the directory listing). Re-running would mean deleting and re-cloning — since network is available locally, it's probably fine, but there's a chance of breaking local state if the network fails. Safer not to delete the local directory. Instead, do a dry check: can run with a temporarily copied script? More simply: trust the fix; it matches the AGENTS.md description ("the download script is in that directory" — `iPlug2/Dependencies/IPlug`).

Can I validate the workflow YAML syntax? Just editing the working-directory string; no problem.

Should I push/commit? The user only requested a fix. Don't commit unless asked. However, CI fixes only take effect once pushed. Mention that. Keep it concise.




Fixed: `download-clap-sdks.sh` lives in `iPlug2/Dependencies/IPlug/` (AGENTS.md notes the scripts are there), but the CI step ran it from `iPlug2/Dependencies/`, so bash failed with "No such file or directory". The step now uses `working-directory: iPlug2/Dependencies/IPlug`. The script's relative clones and `git checkout ./CLAP_*/readme.txt` restore (both readmes are tracked in the submodule) work correctly from that directory.

Commit/push when ready — CI only picks it up after push.

---

