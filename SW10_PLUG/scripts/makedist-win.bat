@echo off
REM - Batch file to build SW10_PLUG Windows distribution.
REM
REM Default path (Phase 5): CMake via CMakePresets.json (vs-win32 / vs-x64, Release).
REM   - requires CMake >= 3.25 and a VS 2026 (or compatible) install
REM   - SDK resolution via SW10_* cache vars / env (VST3_SDK_DIR, VST2_SDK_DIR);
REM     defaults work on the refactor box. VST2 additionally needs aeffect.h/.aeffectx.h
REM     reachable (iPlug2\Dependencies\IPlug\VST2_SDK stub or VST2_SDK_DIR).
REM   - needs ROMSXGM.BIN at SW10_PLUG\build-win\ROMSXGM.BIN (staged next to outputs by
REM     the build; unset SW10_COPY_ROM or point SW10_ROM_PATH elsewhere for ROM-less CI builds).
REM
REM Legacy path:  makedist-win.bat -Legacy [demo]  -> the old msbuild SW10_PLUG.sln flow
REM   (vcxprojs need /p:PlatformToolset=v145 on the current iPlug2 pin — see REFACTOR_PLAN §3;
REM    the vcvarsall path below still points at VS2019 — adjust to your install).
REM
REM Updating version numbers requires python on %PATH%.
REM Installer requires Inno Setup ("Inno Setup 6\iscc.exe" or "Inno Setup 5\iscc.exe").
REM AAX codesigning dropped (AAX support removed, REFACTOR_PLAN §2.3).

setlocal
set "MODE=cmake"
set "ARG1=%~1"
if /I "%ARG1%"=="-Legacy" (
  set "MODE=legacy"
  set "ARG1=%~2"
)

if "%ARG1%"=="1" (echo Making SW10_PLUG Windows DEMO VERSION distribution ...) else (echo Making SW10_PLUG Windows FULL VERSION distribution ...)

echo "touching source"
copy /b ..\*.cpp+,, >nul

echo ------------------------------------------------------------------
echo Updating version numbers ...

call python prepare_resources-win.py %ARG1%
call python update_installer_version.py %ARG1%

cd ..\

if "%MODE%"=="legacy" goto legacy-build

REM ============================== CMake path =========================
echo ------------------------------------------------------------------
echo Building with CMake (VS presets, Release, x64 + Win32) ...

cd ..\

cmake --preset vs-x64
if errorlevel 1 goto fail
cmake --build --preset vs-x64-release
if errorlevel 1 goto fail

cmake --preset vs-win32
if errorlevel 1 goto fail
cmake --build --preset vs-win32-release
if errorlevel 1 goto fail

echo ------------------------------------------------------------------
echo Staging build-cmake outputs into legacy build-win names for the installer ...

REM build-cmake\ is at repo root; build-win\ and installer\ live under SW10_PLUG\.
REM Stage into SW10_PLUG\build-win so the existing installer\SW10_PLUG.iss ("..\build-win\...") works.
if not exist SW10_PLUG\build-win mkdir SW10_PLUG\build-win
copy /y build-cmake\app\Win32\Release\SW10_PLUG.exe  SW10_PLUG\build-win\SW10_PLUG_Win32.exe >nul
copy /y build-cmake\app\x64\Release\SW10_PLUG.exe    SW10_PLUG\build-win\SW10_PLUG_x64.exe  >nul
if exist build-cmake\vst2\Win32\Release\SW10_PLUG.dll copy /y build-cmake\vst2\Win32\Release\SW10_PLUG.dll SW10_PLUG\build-win\SW10_PLUG_Win32.dll >nul
if exist build-cmake\vst2\x64\Release\SW10_PLUG.dll   copy /y build-cmake\vst2\x64\Release\SW10_PLUG.dll   SW10_PLUG\build-win\SW10_PLUG_x64.dll  >nul
if exist SW10_PLUG\build-win\SW10_PLUG.vst3 rmdir /s /q SW10_PLUG\build-win\SW10_PLUG.vst3
xcopy /s /i /y /q build-cmake\vst3\x64\Release\SW10_PLUG.vst3 SW10_PLUG\build-win\SW10_PLUG.vst3 >nul
REM merge the 32-bit binary into the same bundle (installer filters per-arch dirs)
if exist build-cmake\vst3\Win32\Release\SW10_PLUG.vst3\Contents xcopy /s /i /y /q build-cmake\vst3\Win32\Release\SW10_PLUG.vst3\Contents SW10_PLUG\build-win\SW10_PLUG.vst3\Contents >nul

cd SW10_PLUG
goto installer

:legacy-build
REM ============================ Legacy path ==========================
echo ------------------------------------------------------------------
echo Building with msbuild (legacy)...

if not defined DevEnvDir (
  REM NOTE: VS2019 path — on the 2026 box use
  REM   "%ProgramFiles%\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
  REM and add /p:PlatformToolset=v145 to the msbuild lines below.
  if exist "%ProgramFiles(x86)%" (
    call "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_x64
  ) else (
    call "%ProgramFiles%\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_x64
  )
)

REM - set preprocessor macros like this, for instance to enable demo build:
if "%ARG1%"=="1" (
  set CMDLINE_DEFINES="DEMO_VERSION=1"
) else (
  set CMDLINE_DEFINES="DEMO_VERSION=0"
)

echo Building 32 bit binaries...
msbuild SW10_PLUG.sln /p:configuration=release /p:platform=win32 /nologo /verbosity:minimal /fileLogger /m /flp:logfile=build-win.log;errorsonly

echo Building 64 bit binaries...
msbuild SW10_PLUG.sln /p:configuration=release /p:platform=x64 /nologo /verbosity:minimal /fileLogger /m /flp:logfile=build-win.log;errorsonly;append

:installer
REM - Make Installer (InnoSetup)
echo ------------------------------------------------------------------
echo Making Installer ...

set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\iscc.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 6\iscc.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles(x86)%\Inno Setup 5\iscc.exe"
if not exist "%ISCC%" set "ISCC=%ProgramFiles%\Inno Setup 5\iscc.exe"
if exist "%ISCC%" (
  "%ISCC%" /Q /cc ".\installer\SW10_PLUG.iss"
) else (
  echo Inno Setup not found - skipping installer
)

REM - Codesign Installer for Windows 8+
REM -"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin\signtool.exe" sign /f "XXXXX.p12" /p XXXXX /d "SW10_PLUG Installer" ".\installer\SW10_PLUG Installer.exe"

if not "%MODE%"=="legacy" goto done

REM - ZIP (legacy path only; make_zip.py still assumes the build-win/out layout)
echo ------------------------------------------------------------------
echo Making Zip File ...

call python scripts\make_zip.py %ARG1%

echo ------------------------------------------------------------------
echo Printing log file to console...

type build-win.log

:done
echo Done. Outputs under build-cmake\ (CMake) / build-win\ (staged + legacy).
endlocal
exit /b 0

:fail
echo BUILD FAILED
endlocal
exit /b 1
