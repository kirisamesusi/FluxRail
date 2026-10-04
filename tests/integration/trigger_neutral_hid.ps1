param(
 [Parameter(Mandatory=$true)][string]$Port,
 [Parameter(Mandatory=$true)][string]$Serial,
 [Parameter(Mandatory=$true)][string]$Version,
 [Parameter(Mandatory=$true)][string]$BaselineDirectory,
 [Parameter(Mandatory=$true)][string]$OutputDirectory
)
# Two RAM-only transitions, with restoration even if the test fails.
$ErrorActionPreference='Stop'
$utf8=[Text.UTF8Encoding]::new($false)
$base=Get-Content -LiteralPath "$BaselineDirectory/baseline.json" -Raw|ConvertFrom-Json
$profiles=Get-Content -LiteralPath "$BaselineDirectory/original-profiles.json" -Raw|ConvertFrom-Json
if($base.get_device.result.active_profile -ne 4 -or $null -eq $profiles.'1'.motion.hall.enabled -or $null -eq $profiles.'4'.motion.hall.enabled -or $profiles.'1'.motion.hall.enabled -or $profiles.'4'.motion.hall.enabled){throw 'Requires original profile4 and Hall-disabled profiles1/4'}
$p=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$p.ReadTimeout=5000;$p.WriteTimeout=5000;$p.NewLine="`n";$p.DtrEnable=$true
$script:neutralId=30000
$attempted=$false
$result=[ordered]@{started=[DateTime]::UtcNow.ToString('o');original_active=4;ram_only=$true;transitions=@();restored=$false;error=$null}
function Request([string]$Op,$Payload=@{}) {
 $script:neutralId++
 $req=@{protocol_version=1;id=$script:neutralId;op=$Op;payload=$Payload}
 [IO.File]::AppendAllText("$OutputDirectory/action-attempts.jsonl",((@{time=[DateTime]::UtcNow.ToString('o');request=$req}|ConvertTo-Json -Depth 40 -Compress)+"`n"),$utf8)
 $p.WriteLine(($req|ConvertTo-Json -Depth 40 -Compress))
 $reply=$p.ReadLine()|ConvertFrom-Json
 [IO.File]::AppendAllText("$OutputDirectory/action-cdc.jsonl",((@{time=[DateTime]::UtcNow.ToString('o');request=$req;response=$reply}|ConvertTo-Json -Depth 80 -Compress)+"`n"),$utf8)
 if(!$reply.ok -or $reply.id -ne $script:neutralId -or $reply.protocol_version -ne 1){throw "Bad response to $Op"}
 return $reply
}
function Healthy {
 $r=Request 'get_status';$s=$r.result
 if(!$s.config_ready -or $s.applying -or $s.apply_failed -or $s.maintenance -ne 'idle' -or $s.physical_mask -ne 0 -or $r.revision -ne $r.applied_revision -or $s.valid_slots -ne $base.get_status.result.valid_slots -or $r.saved_revision -ne $base.hello.saved_revision){throw 'Board not neutral/healthy'}
 return $r
}
try {
 $p.Open()
 $h=Request 'hello'
 if($h.result.product -ne 'FluxRail' -or $h.result.serial -ne $Serial -or $h.result.firmware_version -ne $Version -or $h.result.usb_mode -ne 'NORMAL_NKRO' -or $h.revision -ne $base.hello.revision){throw 'Identity/revision mismatch'}
 $d=Request 'get_device';if($d.result.active_profile -ne 4){throw 'Active profile changed'}
 $null=Healthy
 foreach($id in @(1,4)){
  $s=Healthy
  $attempted=$true
  $r=Request 'set_active_profile' @{expected_revision=$s.revision;profile_id=$id}
  $d=Request 'get_device';if($d.result.active_profile -ne $id){throw 'Profile apply mismatch'}
  $null=Healthy
  $result.transitions+=@{active_profile=$id;revision=$r.revision;applied_revision=$r.applied_revision;time=[DateTime]::UtcNow.ToString('o')}
 }
} catch {$result.error=$_.Exception.Message;throw}
finally {
 try {
  if($attempted){
   # Fresh revision avoids using a stale revision after a lost response.
   $d=Request 'get_device'
   if($d.result.active_profile -ne 4){$null=Request 'set_active_profile' @{expected_revision=$d.revision;profile_id=4}}
   $d=Request 'get_device';$s=Healthy
   if($d.result.active_profile -ne 4){throw 'Original active profile not restored'}
   $result.restored=$true;$result.final_revision=$s.revision
  }
 } catch {$result.restore_error=$_.Exception.Message;throw}
 finally {
  if($p.IsOpen){$p.Close()};$p.Dispose()
  $result.finished=[DateTime]::UtcNow.ToString('o');$result.serial_closed=$true
  [IO.File]::WriteAllText("$OutputDirectory/action-summary.json",($result|ConvertTo-Json -Depth 12),$utf8)
 }
}
