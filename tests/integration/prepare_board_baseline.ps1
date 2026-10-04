param(
 [Parameter(Mandatory=$true)][string]$Port,
 [Parameter(Mandatory=$true)][string]$Serial,
 [Parameter(Mandatory=$true)][string]$Version,
 [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
if($Port -notmatch '^COM\d+$'){throw 'Explicit COM port required'}
if((Test-Path -LiteralPath $OutputDirectory) -and (Get-ChildItem -Force -LiteralPath $OutputDirectory | Select-Object -First 1)){throw 'Evidence directory must be new or empty'}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$utf8=[Text.UTF8Encoding]::new($false)
$p=[System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$p.ReadTimeout=5000; $p.WriteTimeout=5000; $p.NewLine="`n"; $p.DtrEnable=$true
$script:requestId=20000
function Request([string]$Op,$Payload=@{}) {
 $script:requestId++
 $req=@{protocol_version=1;id=$script:requestId;op=$Op;payload=$Payload}
 $p.WriteLine(($req|ConvertTo-Json -Depth 40 -Compress))
 $reply=$p.ReadLine()|ConvertFrom-Json
 [IO.File]::AppendAllText("$OutputDirectory/baseline-cdc.jsonl", ((@{request=$req;response=$reply;time=[DateTime]::UtcNow.ToString('o')}|ConvertTo-Json -Depth 80 -Compress)+"`n"),$utf8)
 if(!$reply.ok -or $reply.id -ne $script:requestId -or $reply.protocol_version -ne 1){throw "Bad response to $Op"}
 return $reply
}
try {
 $p.Open(); $baseline=[ordered]@{}
 $baseline.hello=Request 'hello'
 if($baseline.hello.result.product -ne 'FluxRail' -or $baseline.hello.result.serial -ne $Serial -or $baseline.hello.result.firmware_version -ne $Version -or $baseline.hello.result.usb_mode -ne 'NORMAL_NKRO'){throw 'Identity mismatch'}
 foreach($op in @('get_status','get_device','list_profiles','get_config','get_motion','get_calibration','get_usb_mode')){$baseline[$op]=Request $op}
 $revision=$baseline.hello.revision
 foreach($reply in $baseline.Values){if($reply.revision -ne $revision){throw 'Concurrent revision change'}}
 $profiles=[ordered]@{}
 foreach($profile in $baseline.list_profiles.result.profiles){$r=Request 'get_profile' @{profile_id=$profile.profile_id};if($r.revision -ne $revision){throw 'Concurrent revision change'};$profiles["$($profile.profile_id)"]=$r.result}
 $bytes=[Collections.Generic.List[byte]]::new(); $offset=0; $length=0; $crc=$null
 do {
  $r=Request 'export_config' @{offset=$offset}; $v=$r.result
  if($offset -eq 0){$length=$v.length;$crc=$v.crc32}
  if($r.revision -ne $revision -or $v.schema -ne 2 -or $v.offset -ne $offset -or $v.length -ne $length -or $v.crc32 -ne $crc){throw 'Inconsistent export'}
  if($v.data.Length -eq 0 -or $v.data.Length % 2 -ne 0){throw 'Invalid hex chunk'}
  for($i=0;$i -lt $v.data.Length;$i+=2){$bytes.Add([Convert]::ToByte($v.data.Substring($i,2),16))}
  $offset=$bytes.Count
  if($v.next_offset -ne $offset -or $offset -gt $length){throw 'Invalid chunk boundary'}
 } while($offset -lt $length)
 $baseline.export_metadata=@{revision=$revision;length=$length;crc32=$crc}
 [IO.File]::WriteAllBytes("$OutputDirectory/original-schema2.bin",$bytes.ToArray())
 [IO.File]::WriteAllText("$OutputDirectory/original-profiles.json",($profiles|ConvertTo-Json -Depth 80),$utf8)
 [IO.File]::WriteAllText("$OutputDirectory/baseline.json",($baseline|ConvertTo-Json -Depth 80),$utf8)
 Write-Output (@{serial=$Serial;version=$Version;revision=$revision;length=$length;crc32=$crc;profiles=$profiles.Count}|ConvertTo-Json -Compress)
} finally {if($p.IsOpen){$p.Close()};$p.Dispose()}
