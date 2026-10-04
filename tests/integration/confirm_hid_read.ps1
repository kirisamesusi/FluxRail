param(
 [Parameter(Mandatory=$true)][string]$ObservedPaths,
 [Parameter(Mandatory=$true)][string]$Serial,
 [Parameter(Mandatory=$true)][string]$Output,
 [switch]$OpenOnly
)
# Independent C# binding; one read-only current-report request to observed COL02.
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $Output){throw 'Refusing to overwrite evidence'}
$observed=Get-Content -Raw -Encoding UTF8 -LiteralPath $ObservedPaths | ConvertFrom-Json
$target=@($observed | Where-Object { $_.serial -eq $Serial -and $_.path -match 'vid_239a&pid_cafe.*col02' })
if($target.Count -ne 1 -or $target[0].path -isnot [string]){throw 'Exactly one previously observed target required'}
$devicePath=$target[0].path
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FluxRailReadOnlyHid {
 [DllImport("kernel32.dll", EntryPoint="CreateFileW", ExactSpelling=true, CharSet=CharSet.Unicode, SetLastError=true)]
 public static extern IntPtr Open(string p, uint access, uint share, IntPtr security, uint disposition, uint flags, IntPtr template);
 [DllImport("kernel32.dll", SetLastError=true)]
 [return: MarshalAs(UnmanagedType.Bool)] public static extern bool CloseHandle(IntPtr h);
 [DllImport("hid.dll", SetLastError=true)]
 [return: MarshalAs(UnmanagedType.U1)] public static extern bool HidD_GetSerialNumberString(IntPtr h, IntPtr b, uint n);
 [DllImport("hid.dll", SetLastError=true)]
 [return: MarshalAs(UnmanagedType.U1)] public static extern bool HidD_GetPreparsedData(IntPtr h, out IntPtr b);
 [DllImport("hid.dll")]
 public static extern int HidP_GetCaps(IntPtr pp, IntPtr caps);
 [DllImport("hid.dll")]
 [return: MarshalAs(UnmanagedType.U1)] public static extern bool HidD_FreePreparsedData(IntPtr pp);
 [DllImport("hid.dll", SetLastError=true)]
 [return: MarshalAs(UnmanagedType.U1)] public static extern bool HidD_GetInputReport(IntPtr h, [In, Out] byte[] b, uint n);
}
'@
$h=[FluxRailReadOnlyHid]::Open($devicePath,[uint32]0,[uint32]3,[IntPtr]::Zero,[uint32]3,[uint32]0,[IntPtr]::Zero)
$openError=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
$result=[ordered]@{started=[DateTime]::UtcNow.ToString('o');path=$devicePath;desired_access=0;method='Independent C# HidD_GetInputReport; COL02 single current-state query';control_requests=0}
if($h -eq [IntPtr]::new(-1)){
 $result.open_error=$openError
 [IO.File]::WriteAllText([IO.Path]::GetFullPath($Output),($result|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
 throw "CreateFile error $openError"
}
if($OpenOnly){
 $result.handle_closed=[FluxRailReadOnlyHid]::CloseHandle($h)
 $result.method='C# corrected binding metadata-open only'
 [IO.File]::WriteAllText([IO.Path]::GetFullPath($Output),($result|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
 $result|ConvertTo-Json -Depth 8
 exit
}
$pp=[IntPtr]::Zero; $text=[Runtime.InteropServices.Marshal]::AllocHGlobal(252); $caps=[Runtime.InteropServices.Marshal]::AllocHGlobal(64)
try {
 if(![FluxRailReadOnlyHid]::HidD_GetSerialNumberString($h,$text,252)){throw 'Serial read failed'}
 $result.serial=[Runtime.InteropServices.Marshal]::PtrToStringUni($text)
 if($result.serial -ne $Serial){throw 'Serial mismatch'}
 if(![FluxRailReadOnlyHid]::HidD_GetPreparsedData($h,[ref]$pp)){throw 'Preparsed data failed'}
 $status=[FluxRailReadOnlyHid]::HidP_GetCaps($pp,$caps)
 if($status -ne 0x00110000){throw 'Caps failed'}
 $result.usage=[Runtime.InteropServices.Marshal]::ReadInt16($caps,0)
 $result.usage_page=[Runtime.InteropServices.Marshal]::ReadInt16($caps,2)
 $result.length=[Runtime.InteropServices.Marshal]::ReadInt16($caps,4)
 if($result.usage -ne 2 -or $result.usage_page -ne 1 -or $result.length -ne 6){throw 'COL02 caps mismatch'}
 $buffer=[byte[]]::new($result.length);$buffer[0]=2
 $result.control_requests=1
 $result.passed=[FluxRailReadOnlyHid]::HidD_GetInputReport($h,$buffer,$buffer.Length)
 $apiError=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
 $result.buffer_is_device_report=$result.passed
 $result.hex=if($result.passed){[BitConverter]::ToString($buffer).Replace('-','').ToLowerInvariant()}else{$null}
 if(!$result.passed){$result.error=$apiError;$result.error_message=[ComponentModel.Win32Exception]::new($apiError).Message;$result.failed_buffer_hex=[BitConverter]::ToString($buffer).Replace('-','').ToLowerInvariant()}
} finally {
 if($pp -ne [IntPtr]::Zero){[void][FluxRailReadOnlyHid]::HidD_FreePreparsedData($pp)}
 [Runtime.InteropServices.Marshal]::FreeHGlobal($text)
 [Runtime.InteropServices.Marshal]::FreeHGlobal($caps)
 $result.handle_closed=[FluxRailReadOnlyHid]::CloseHandle($h)
 $result.finished=[DateTime]::UtcNow.ToString('o')
 [IO.File]::WriteAllText([IO.Path]::GetFullPath($Output),($result|ConvertTo-Json -Depth 8),[Text.UTF8Encoding]::new($false))
}
$result|ConvertTo-Json -Depth 8
