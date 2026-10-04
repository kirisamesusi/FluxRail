param(
 [Parameter(Mandatory=$true)][string]$EvidenceRoot,
 [string]$Serial='5054316560DD729C',
 [string]$Version='0.10.1'
)
# Authorized storage test. Leaves original configuration saved, not empty Flash.
$ErrorActionPreference='Stop'
$utf8=[Text.UTF8Encoding]::new($false)
$initial=Get-Content -LiteralPath "$EvidenceRoot/initial/baseline.json" -Raw|ConvertFrom-Json
$profiles=Get-Content -LiteralPath "$EvidenceRoot/initial/original-profiles.json" -Raw|ConvertFrom-Json
if($initial.get_device.result.active_profile -ne 4 -or $profiles.'1'.motion.hall.enabled -ne $false){throw 'Unexpected initial profile or Hall setting'}
$out="$EvidenceRoot/flash-normal"
if(Test-Path -LiteralPath $out){throw 'Fresh stage directory required'}
New-Item -ItemType Directory -Path $out|Out-Null
$script:flashId=40000;$script:p=$null
$mutated=$false;$savedOriginal=$false
$result=[ordered]@{started=[DateTime]::UtcNow.ToString('o');saves=@();reboots=@();restored=$false;error=$null;physical_power_cycle=$false}
function ClosePort {
 if($script:p){try {if($script:p.IsOpen){$script:p.Close()}}finally{$script:p.Dispose();$script:p=$null}}
}
function OpenPort {
 $deadline=[DateTime]::UtcNow.AddSeconds(20)
 do {
  $ports=@(Get-CimInstance Win32_SerialPort|Where-Object PNPDeviceID -Like 'USB\VID_239A&PID_CAFE&MI_00\*')
  if($ports.Count -gt 1){throw 'FluxRail NORMAL CDC port ambiguous'}
  if($ports.Count -eq 1){break}
  Start-Sleep -Milliseconds 400
 } while([DateTime]::UtcNow -lt $deadline)
 if($ports.Count -ne 1){throw 'FluxRail NORMAL CDC did not re-enumerate'}
 $script:p=[IO.Ports.SerialPort]::new($ports[0].DeviceID,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
 $script:p.ReadTimeout=12000;$script:p.WriteTimeout=5000;$script:p.NewLine="`n";$script:p.DtrEnable=$true
 $script:p.Open()
 $r=Request 'hello'
 if($r.result.product -ne 'FluxRail' -or $r.result.serial -ne $Serial -or $r.result.firmware_version -ne $Version -or $r.result.usb_mode -ne 'NORMAL_NKRO'){throw 'Identity mismatch'}
 return $r
}
function Request([string]$Op,$Payload=@{}) {
 $script:flashId++;$req=@{protocol_version=1;id=$script:flashId;op=$Op;payload=$Payload}
 $start=[DateTime]::UtcNow
 [IO.File]::AppendAllText("$out/attempts.jsonl",((@{time=$start.ToString('o');request=$req}|ConvertTo-Json -Depth 30 -Compress)+"`n"),$utf8)
 $script:p.WriteLine(($req|ConvertTo-Json -Depth 30 -Compress))
 $reply=$script:p.ReadLine()|ConvertFrom-Json
 [IO.File]::AppendAllText("$out/cdc.jsonl",((@{time=[DateTime]::UtcNow.ToString('o');elapsed_ms=([DateTime]::UtcNow-$start).TotalMilliseconds;request=$req;response=$reply}|ConvertTo-Json -Depth 70 -Compress)+"`n"),$utf8)
 if(!$reply.ok -or $reply.id -ne $script:flashId -or $reply.protocol_version -ne 1){throw "Failed $Op"}
 return $reply
}
function Healthy {
 $s=Request 'get_status'
 if(!$s.result.config_ready -or $s.result.applying -or $s.result.apply_failed -or $s.result.maintenance -ne 'idle' -or $s.result.physical_mask -ne 0 -or $s.revision -ne $s.applied_revision){throw 'Not neutral/healthy'}
 return $s
}
function Save {
 $s=Healthy
 $r=Request 'save_config' @{expected_revision=$s.revision}
 $s=Healthy
 if($r.saved_revision -ne $r.revision -or $r.dirty -or $s.result.save_result -ne 1 -or $s.saved_revision -ne $s.revision -or $s.dirty){throw 'Save not confirmed healthy'}
 $result.saves+=@{revision=$r.revision;valid_slots_mask=$s.result.valid_slots;time=[DateTime]::UtcNow.ToString('o')}
 return $r
}
function Snapshot([string]$Name) {
 $port=$script:p.PortName;ClosePort
 & "$PSScriptRoot/prepare_board_baseline.ps1" -Port $port -Serial $Serial -Version $Version -OutputDirectory "$out/$Name"
 $null=OpenPort
}
function Reboot([string]$Name) {
 $before=Request 'get_motion';$s=Healthy
 $r=Request 'reboot_to_saved_mode' @{expected_revision=$s.revision;target='NORMAL_NKRO'}
 if(!$r.result.reboot_ready){throw 'Reboot response not ready'}
 # Retain DTR after the delivered response so the app can complete detach.
 Start-Sleep -Milliseconds 450
 ClosePort
 Start-Sleep -Milliseconds 650
 $h=OpenPort;$after=Request 'get_motion';$null=Healthy
 if($h.saved_revision -ne $h.revision -or $h.dirty -or ($after.result.timestamp_us -ge $before.result.timestamp_us -and $after.result.epoch -ge $before.result.epoch)){throw 'Application reboot/load not established'}
 $result.reboots+=@{name=$Name;before_timestamp_us=$before.result.timestamp_us;after_timestamp_us=$after.result.timestamp_us;before_epoch=$before.result.epoch;after_epoch=$after.result.epoch;revision=$h.revision;application_reboot_only=$true;time=[DateTime]::UtcNow.ToString('o')}
}
try {
 $h=OpenPort
 if($h.revision -ne $initial.hello.revision){throw 'Concurrent revision change'}
 $null=Healthy
 $null=Save;$savedOriginal=$true
 $s=Healthy;$mutated=$true
 $null=Request 'set_active_profile' @{expected_revision=$s.revision;profile_id=1}
 # 50 successful maintenance saves total, including initial baseline save.
 for($i=1;$i -lt 50;$i++){$null=Save}
 Snapshot 'saved-marker'
 Reboot 'marker-load'
 Snapshot 'loaded-marker'
 $d=Request 'get_device';if($d.result.active_profile -ne 1){throw 'Saved non-default active profile not loaded'}
 $a=[IO.File]::ReadAllBytes("$out/saved-marker/original-schema2.bin")
 $b=[IO.File]::ReadAllBytes("$out/loaded-marker/original-schema2.bin")
 if([Convert]::ToBase64String($a) -ne [Convert]::ToBase64String($b)){throw 'Saved marker snapshot not restored byte-exact'}
 $result.marker_loaded=$true
} catch {$result.error=$_.Exception.Message;throw}
finally {
 try {
  if($mutated -and $savedOriginal){
   if(!$script:p -or !$script:p.IsOpen){$null=OpenPort}
   $d=Request 'get_device'
   if($d.result.active_profile -ne 4){$null=Request 'set_active_profile' @{expected_revision=$d.revision;profile_id=4}}
   $null=Save
   Reboot 'original-load'
   Snapshot 'final'
   $d=Request 'get_device';$s=Healthy
   $original=[IO.File]::ReadAllBytes("$EvidenceRoot/initial/original-schema2.bin")
   $final=[IO.File]::ReadAllBytes("$out/final/original-schema2.bin")
   if($original.Length -ne $final.Length){throw 'Restoration length changed'}
   for($i=0;$i -lt $original.Length;$i++){if(($i -lt 4 -or $i -gt 7) -and $original[$i] -ne $final[$i]){throw "Restoration differs at $i"}}
   # Normalize parsed JSON in this host: pwsh7 and Windows PS5 indent/number formats differ.
   $originalProfiles=Get-Content -LiteralPath "$EvidenceRoot/initial/original-profiles.json" -Raw|ConvertFrom-Json
   $finalProfiles=Get-Content -LiteralPath "$out/final/original-profiles.json" -Raw|ConvertFrom-Json
   if(($originalProfiles|ConvertTo-Json -Depth 80 -Compress) -ne ($finalProfiles|ConvertTo-Json -Depth 80 -Compress)){throw 'Profiles not restored'}
   if($d.result.active_profile -ne 4 -or $s.saved_revision -ne $s.revision -or $s.dirty -or $s.result.valid_slots -ne 3){throw 'Final saved state mismatch'}
   $result.restored=$true;$result.final_revision=$s.revision;$result.final_valid_slots_mask=$s.result.valid_slots
  }
 } catch {$result.restore_error=$_.Exception.Message;throw}
 finally {
  ClosePort;$result.serial_closed=$true;$result.finished=[DateTime]::UtcNow.ToString('o')
  [IO.File]::WriteAllText("$out/summary.json",($result|ConvertTo-Json -Depth 20),$utf8)
 }
}
