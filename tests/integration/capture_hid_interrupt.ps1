param(
 [Parameter(Mandatory=$true)][string]$Port,
 [Parameter(Mandatory=$true)][string]$Serial,
 [Parameter(Mandatory=$true)][string]$Version,
 [Parameter(Mandatory=$true)][string]$OutputDirectory,
 [Parameter(Mandatory=$true)][string]$Python,
 [switch]$CheckToolOnly
)
# Neutral interrupt capture covers the observed Intel xHCI/ROOT_HUB30 target device chain.
# Built-in ETW only, no driver installation or self-elevation.
# Requires administrator terminal. Captures local USB events in a bounded file.
$ErrorActionPreference='Stop'
$taskLogman=(Get-Command logman.exe -CommandType Application -ErrorAction Stop).Source
$events=[Collections.Generic.List[object]]::new()
function InvokeUsbTraceCommand([string[]]$CommandArgs) {
 $message=(& $taskLogman @CommandArgs 2>&1|Out-String);$code=$LASTEXITCODE
 $events.Add(@{time=[DateTime]::UtcNow.ToString('o');arguments=$CommandArgs;output=$message;exit=$code})
 if($code -ne 0){throw "logman failed $code"}
}
if($CheckToolOnly){
 # No administrator, trace session or device access needed for executable check.
 InvokeUsbTraceCommand -CommandArgs @('query','providers','Microsoft-Windows-USB-USBXHCI')
 @{native_executable=$taskLogman;check_only=$true;commands=@($events.ToArray())}|ConvertTo-Json -Depth 10
 return
}
$principal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if(!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'USB ETW requires administrator; this script does not elevate itself'}
if(Test-Path -LiteralPath $OutputDirectory){throw 'Evidence directory must not exist'}
if(!(Test-Path -LiteralPath $Python -PathType Leaf)){throw 'Explicit Python executable required'}
$out=[IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $out | Out-Null
$session='FluxRailNeutral_'+[Guid]::NewGuid().ToString('N')
$started=$false
try {
 & "$PSScriptRoot/prepare_board_baseline.ps1" -Port $Port -Serial $Serial -Version $Version -OutputDirectory "$out/before"
 & $Python "$PSScriptRoot/windows_hid_probe.py" --serial $Serial --output "$out/current-paths.json"
 if($LASTEXITCODE -ne 0){throw 'Present HID metadata check failed'}
 # Local provider inventory: Default=1, PartialDataBusTrace=0x80, Rundown=0x8000.
 # Target USB2 device is observed below Intel USB3 xHCI/ROOT_HUB30.
 # Capture bounded partial transfer payload + device context from all three USB3 stack layers.
 # Partial payload length must be verified before claiming report readback.
 InvokeUsbTraceCommand -CommandArgs @('create','trace',$session,'-o',"$out/usb.etl",'-f','bincirc','-max','16','-bs','64','-nb','16','64','-p','Microsoft-Windows-USB-USBXHCI','0x8081','5','-ets')
 $started=$true
 InvokeUsbTraceCommand -CommandArgs @('update','trace',$session,'-p','Microsoft-Windows-USB-UCX','0x8081','5','-ets')
 InvokeUsbTraceCommand -CommandArgs @('update','trace',$session,'-p','Microsoft-Windows-USB-USBHUB3','0x8081','5','-ets')
 InvokeUsbTraceCommand -CommandArgs @('query',$session,'-ets')
 try {
  & "$PSScriptRoot/trigger_neutral_hid.ps1" -Port $Port -Serial $Serial -Version $Version -BaselineDirectory "$out/before" -OutputDirectory $out
 } finally {
  # Action restores original profile before this full configuration comparison.
  & "$PSScriptRoot/prepare_board_baseline.ps1" -Port $Port -Serial $Serial -Version $Version -OutputDirectory "$out/after"
 }
} finally {
 if($started){
  $message=(& $taskLogman stop $session -ets 2>&1|Out-String)
  $stopCode=$LASTEXITCODE
  $events.Add(@{time=[DateTime]::UtcNow.ToString('o');arguments=@('stop',$session,'-ets');output=$message;exit=$stopCode})
 }
 [IO.File]::WriteAllText("$out/capture-commands.json",(ConvertTo-Json -InputObject @($events.ToArray()) -Depth 10),[Text.UTF8Encoding]::new($false))
 if($started -and $stopCode -ne 0){throw "ETW stop failed ($stopCode); inspect own session $session before continuing"}
}
# Raw ETL remains authoritative. XML decoding success alone is not bus validation.
& tracerpt "$out/usb.etl" -lr -of XML -o "$out/usb-events.xml" -summary "$out/trace-summary.txt" -y
if($LASTEXITCODE -ne 0){throw 'tracerpt failed; preserve ETL for later analysis'}
$before=[IO.File]::ReadAllBytes("$out/before/original-schema2.bin")
$after=[IO.File]::ReadAllBytes("$out/after/original-schema2.bin")
if($before.Length -ne $after.Length){throw 'Configuration length changed'}
# Schema2 revision is four little-endian bytes at offsets4..7; compare everything else.
for($i=0;$i -lt $before.Length;$i++){if(($i -lt 4 -or $i -gt 7) -and $before[$i] -ne $after[$i]){throw "Configuration changed at offset $i"}}
$beforeProfiles=Get-Content -LiteralPath "$out/before/original-profiles.json" -Raw
$afterProfiles=Get-Content -LiteralPath "$out/after/original-profiles.json" -Raw
if($beforeProfiles -ne $afterProfiles){throw 'Profiles changed'}
Write-Output "Captured to $out. Requires review of target USB setup/status and positive CDC traffic before drawing conclusions."
