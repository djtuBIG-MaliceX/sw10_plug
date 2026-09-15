<#
.SYNOPSIS
  SW10_PLUG plugin load-test harness (Phase 4). No DAW required.
  Loads each built plugin binary with the ROM staged next to it and walks the
  format entry points: VST2 (VSTPluginMain -> AEffect magic/version, effOpen/effClose),
  VST3 (ModuleInit/GetPluginFactory -> countClasses/getClassInfo), CLAP (clap_entry
  version/init/factory_get/deinit). Load only - instantiation without audio I/O;
  "makes sound" still needs an interactive host (REAPER not installed).
.DESCRIPTION
  Run once per arch:  powershell -File loadtest.ps1 -Arch x64|Win32 [-Root <build-cmake>]
  Win32 artifacts need a 32-bit PowerShell; the script re-launches itself under
  SysWOW64 Windows PowerShell automatically. Exit code = number of failures.
#>
param(
  [ValidateSet('x64','Win32')][string]$Arch = 'x64',
  [string]$ArchDir = '',
  [string]$Root = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'build-cmake'),
  [switch]$NoRelaunch
)

$ErrorActionPreference = 'Stop'
$want64 = ($Arch -eq 'x64')
if (-not $ArchDir) { $ArchDir = $Arch }   # MinGW artifacts live in <arch>-mingw; pass -ArchDir x64-mingw
if (([Environment]::Is64BitProcess -ne $want64) -and -not $NoRelaunch) {
  if ($want64) {
    Write-Error "this shell is 32-bit but -Arch x64 requested"; exit 1
  }
  $ps32 = Join-Path $env:SystemRoot 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
  & $ps32 -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Arch $Arch -ArchDir $ArchDir -Root $Root -NoRelaunch
  exit $LASTEXITCODE
}
if (([Environment]::Is64BitProcess -ne $want64)) { Write-Error "bitness mismatch after relaunch"; exit 1 }

Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;

public static class LoadTest
{
  [DllImport("kernel32", SetLastError = true)] public static extern IntPtr LoadLibrary(string name);
  [DllImport("kernel32")] public static extern IntPtr GetProcAddress(IntPtr h, string name);
  [DllImport("kernel32")] public static extern bool FreeLibrary(IntPtr h);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr HostCallback(IntPtr effect, int opcode, int index, IntPtr value, IntPtr ptr, float opt);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr Dispatcher(IntPtr effect, int opcode, int index, IntPtr value, IntPtr ptr, float opt);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr Vst2Main(HostCallback host);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  [return: MarshalAs(UnmanagedType.I1)]
  delegate bool ModuleInit(IntPtr hInst);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  [return: MarshalAs(UnmanagedType.I1)]
  delegate bool ModuleNoArg();

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr GetFactory();

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate int CountClasses(IntPtr self);

  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  delegate int CountClasses32(IntPtr self);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate int GetClassInfo(IntPtr self, int index, byte[] info);

  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  delegate int GetClassInfo32(IntPtr self, int index, byte[] info);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate void ModuleExit();

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  [return: MarshalAs(UnmanagedType.I1)]
  delegate bool ClapInit(string path);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate void ClapDeinit();

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate IntPtr ClapGetFactory([MarshalAs(UnmanagedType.LPStr)] string factoryId);

  [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
  delegate uint ReleaseRef(IntPtr self);

  [UnmanagedFunctionPointer(CallingConvention.StdCall)]
  delegate uint ReleaseRef32(IntPtr self);

  static HostCallback _host = HostStub; // root the delegate
  static IntPtr HostStub(IntPtr e, int op, int idx, IntPtr v, IntPtr p, float o) { return IntPtr.Zero; }

  static IntPtr PtrAt(IntPtr base_, int index)
  {
    return Marshal.ReadIntPtr(base_, index * IntPtr.Size);
  }

  public static string TestVst2(string path)
  {
    IntPtr h = LoadLibrary(path);
    if (h == IntPtr.Zero) return "FAIL LoadLibrary err=" + Marshal.GetLastWin32Error();
    try
    {
      IntPtr pMain = GetProcAddress(h, "VSTPluginMain");
      if (pMain == IntPtr.Zero) pMain = GetProcAddress(h, "main");
      if (pMain == IntPtr.Zero) return "FAIL no VSTPluginMain/main export";
      var main = Marshal.GetDelegateForFunctionPointer<Vst2Main>(pMain);
      IntPtr e = main(_host);
      if (e == IntPtr.Zero) return "FAIL VSTPluginMain returned null AEffect";
      int ptr = IntPtr.Size;
      int magic = Marshal.ReadInt32(e, 0);
      long dispOff = 8;                    // magic int32 + padding (x64) / +4 (x86)
      if (ptr == 4) dispOff = 4;
      int uniqueOff = ptr == 8 ? 112 : 72;
      int versionOff = ptr == 8 ? 116 : 76;
      int unique = Marshal.ReadInt32(e, uniqueOff);
      int version = Marshal.ReadInt32(e, versionOff);
      var disp = Marshal.GetDelegateForFunctionPointer<Dispatcher>(Marshal.ReadIntPtr(e, (int)dispOff));
      disp(e, 1, 0, IntPtr.Zero, IntPtr.Zero, 0f);   // effOpen
      IntPtr gotVer = disp(e, 11, 0, IntPtr.Zero, IntPtr.Zero, 0f); // effGetVersion
      disp(e, 2, 0, IntPtr.Zero, IntPtr.Zero, 0f);   // effClose (instance torn down)
      if (magic != 0x56737450) return string.Format("FAIL AEffect magic=0x{0:X8} != 'VstP'", magic);
      if (Marshal.ReadIntPtr(e, (int)dispOff) == IntPtr.Zero) return "FAIL null dispatcher";
      return string.Format("PASS magic=VstP unique=0x{0:X8} version={1} effGetVersion={2}", unique, version, gotVer.ToInt64());
    }
    finally { FreeLibrary(h); }
  }

  public static string TestVst3(string path)
  {
    IntPtr h = LoadLibrary(path);
    if (h == IntPtr.Zero) return "FAIL LoadLibrary err=" + Marshal.GetLastWin32Error();
    try
    {
      IntPtr pInit = GetProcAddress(h, "ModuleInit");
      IntPtr pExit = GetProcAddress(h, "ModuleExit");
      if (pInit == IntPtr.Zero) { pInit = GetProcAddress(h, "InitDll"); pExit = GetProcAddress(h, "ExitDll"); }
      IntPtr pFac = GetProcAddress(h, "GetPluginFactory");
      if (pFac == IntPtr.Zero) pFac = GetProcAddress(h, "_GetPluginFactory@0"); // x86 PLUGIN_API==stdcall
      if (pInit == IntPtr.Zero || pFac == IntPtr.Zero) return "FAIL ModuleInit/InitDll or GetPluginFactory export missing";
      bool ok;
      if (pInit == GetProcAddress(h, "InitDll"))
        ok = Marshal.GetDelegateForFunctionPointer<ModuleNoArg>(pInit)();
      else
        ok = Marshal.GetDelegateForFunctionPointer<ModuleInit>(pInit)(h);
      if (!ok) return "FAIL ModuleInit/InitDll returned false";
      IntPtr fac = Marshal.GetDelegateForFunctionPointer<GetFactory>(pFac)();
      if (fac == IntPtr.Zero) return "FAIL GetPluginFactory null";
      try
      {
        IntPtr vtbl = Marshal.ReadIntPtr(fac);
        int n; byte[] buf = new byte[512]; int rc;
        // vst3sdk PLUGIN_API is __stdcall on x86, cdecl on x64
        if (IntPtr.Size == 4)
        {
          var count = Marshal.GetDelegateForFunctionPointer<CountClasses32>(PtrAt(vtbl, 4)); // getFactoryInfo(3), countClasses(4)
          n = count(fac);
          var info = Marshal.GetDelegateForFunctionPointer<GetClassInfo32>(PtrAt(vtbl, 5));
          rc = info(fac, 0, buf);
        }
        else
        {
          var count = Marshal.GetDelegateForFunctionPointer<CountClasses>(PtrAt(vtbl, 4)); // getFactoryInfo(3), countClasses(4)
          n = count(fac);
          var info = Marshal.GetDelegateForFunctionPointer<GetClassInfo>(PtrAt(vtbl, 5));
          rc = info(fac, 0, buf);
        }
        if (n < 1 || rc != 0) return string.Format("FAIL countClasses={0} getClassInfo rc={1}", n, rc);
        string raw = System.Text.Encoding.ASCII.GetString(buf);
        int idx = raw.IndexOf("SW10");
        string peek = idx >= 0 ? raw.Substring(idx, 40) : raw.Substring(0, 48);
        peek = System.Text.RegularExpressions.Regex.Replace(peek, "[^\\x20-\\x7e]", ".");
        return string.Format("PASS factory classes={0} class0-info='{1}'", n, peek);
      }
      finally
      {
        // Release (vtable index 2) the factory, then ModuleExit
        IntPtr vtbl2 = Marshal.ReadIntPtr(fac);
        try
        {
          if (IntPtr.Size == 4)
            Marshal.GetDelegateForFunctionPointer<ReleaseRef32>(PtrAt(vtbl2, 2))(fac);
          else
            Marshal.GetDelegateForFunctionPointer<ReleaseRef>(PtrAt(vtbl2, 2))(fac);
        }
        catch { }
        if (pExit != IntPtr.Zero) Marshal.GetDelegateForFunctionPointer<ModuleExit>(pExit)();
      }
    }
    finally { FreeLibrary(h); }
  }

  public static string TestClap(string path)
  {
    IntPtr h = LoadLibrary(path);
    if (h == IntPtr.Zero) return "FAIL LoadLibrary err=" + Marshal.GetLastWin32Error();
    try
    {
      IntPtr entry = GetProcAddress(h, "clap_entry");
      if (entry == IntPtr.Zero) return "FAIL no clap_entry export";
      uint major = (uint)Marshal.ReadInt32(entry, 0);
      uint minor = (uint)Marshal.ReadInt32(entry, 4);
      int ptr = IntPtr.Size;
      int initOff = ptr == 8 ? 16 : 12; // clap_version_t is 12 bytes, padded to 8-align on x64
      var init = Marshal.GetDelegateForFunctionPointer<ClapInit>(Marshal.ReadIntPtr(entry, initOff));
      if (!init(path)) return "FAIL clap init() returned false";
      try
      {
        string extra = "";
        IntPtr pGet = Marshal.ReadIntPtr(entry, initOff + 2 * ptr); // get_factory (after init, deinit)
        if (pGet != IntPtr.Zero)
        {
          var gf = Marshal.GetDelegateForFunctionPointer<ClapGetFactory>(pGet);
          // CLAP 1.2 factory id is "clap.plugin-factory" (older drafts: "clap.plugin")
          IntPtr fac = gf("clap.plugin-factory");
          string fid = "clap.plugin-factory";
          if (fac == IntPtr.Zero) { fac = gf("clap.plugin"); fid = "clap.plugin"; }
          extra = fac != IntPtr.Zero ? " get_factory(" + fid + ")=ok" : " get_factory=NULL";
        }
        return string.Format("PASS clap_version={0}.{1} init=ok{2}", major, minor, extra);
      }
      finally
      {
        IntPtr pDe = Marshal.ReadIntPtr(entry, initOff + ptr);
        if (pDe != IntPtr.Zero) Marshal.GetDelegateForFunctionPointer<ClapDeinit>(pDe)();
      }
    }
    finally { FreeLibrary(h); }
  }

  public static bool StageRom(string binDir, string romPath)
  {
    string dst = Path.Combine(binDir, "ROMSXGM.BIN");
    if (File.Exists(dst)) return true;
    if (romPath != null && File.Exists(romPath)) { File.Copy(romPath, dst); return true; }
    return false;
  }
}
'@

$results = @()
function Run-Case([string]$api, [string]$path, [scriptblock]$test) {
  $binDir = Split-Path -Parent $path
  $rom = Join-Path $binDir 'ROMSXGM.BIN'
  if (-not (Test-Path $rom)) {
    $romSrc = Join-Path (Split-Path -Parent (Split-Path -Parent $Root)) 'SW10_PLUG\build-win\ROMSXGM.BIN'
    if (Test-Path $romSrc) { Copy-Item $romSrc $rom }
  }
  if (-not (Test-Path $path)) { $script:results += "SKIP $api $Arch (missing $path)"; return }
  $detail = & $test
  $script:results += "{0} {1} {2} {3}" -f ($detail.Split(' ')[0]), $api, $Arch, $detail.Substring($detail.IndexOf(' ')+1) + " [$path]"
}

Write-Host "SW10 loadtest ($Arch, dir=$ArchDir, root=$Root)"
Run-Case 'vst2' (Join-Path $Root "vst2\$ArchDir\Release\SW10_PLUG.dll")   { [LoadTest]::TestVst2($path) }
Run-Case 'vst3' (Join-Path $Root "vst3\$ArchDir\Release\SW10_PLUG.vst3\Contents\$(if ($Arch -eq 'x64') {'x86_64-win'} else {'x86-win'})\SW10_PLUG.vst3") { [LoadTest]::TestVst3($path) }
Run-Case 'clap' (Join-Path $Root "clap\$ArchDir\Release\SW10_PLUG.clap")  { [LoadTest]::TestClap($path) }
$results | ForEach-Object { Write-Host $_ }
$fail = ($results | Where-Object { $_ -like 'FAIL*' }).Count
exit $fail
