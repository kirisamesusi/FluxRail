param([Parameter(Mandatory=$true)][string]$ObservedPaths,[Parameter(Mandatory=$true)][string]$Output)
# Opens only observed COL02 access0, closes it; no report requests.
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Refusing to overwrite evidence'}
$observed=Get-Content -Raw -Encoding UTF8 -LiteralPath $ObservedPaths | ConvertFrom-Json
$target=@($observed | Where-Object { $_.serial -eq '5054316560DD729C' -and $_.path -match 'col02' })
if($target.Count -ne 1 -or $target[0].path -isnot [string]){throw 'Bad target'}
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class FluxRailOpenOnly {
 [DllImport("kernel32.dll", EntryPoint="CreateFileW", ExactSpelling=true, CharSet=CharSet.Unicode, SetLastError=true)]
 public static extern IntPtr Open(string p, uint a, uint s, IntPtr sec, uint d, uint f, IntPtr t);
 [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
 public static string FixedOpen(string p) {
   IntPtr h=Open(p,0,3,IntPtr.Zero,3,0,IntPtr.Zero);
   int e=Marshal.GetLastWin32Error();
   bool closed=h!=new IntPtr(-1) && CloseHandle(h);
   return "handle="+h.ToInt64()+";error="+e+";closed="+closed;
 }
}
'@
$path=$target[0].path
$result=@{path=$path;path_length=$path.Length;process64=[Environment]::Is64BitProcess;os64=[Environment]::Is64BitOperatingSystem;ps_version=$PSVersionTable.PSVersion.ToString();
 csharp_fixed=[FluxRailOpenOnly]::FixedOpen($path);controls=0}
$h=[FluxRailOpenOnly]::Open($path,[uint32]0,[uint32]3,[IntPtr]::Zero,[uint32]3,[uint32]0,[IntPtr]::Zero)
$apiError=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
$result.ps_direct=@{invalid=$h -eq [IntPtr]::new(-1);error=$apiError;closed=$false}
if($h -ne [IntPtr]::new(-1)){$result.ps_direct.closed=[FluxRailOpenOnly]::CloseHandle($h)}
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Output),($result|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
$result|ConvertTo-Json -Depth 8
