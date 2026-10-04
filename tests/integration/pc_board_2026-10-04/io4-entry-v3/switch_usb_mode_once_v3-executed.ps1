param([Parameter(Mandatory=$true)][ValidateSet('IO4','NORMAL_NKRO')][string]$Target,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop';$utf8=[Text.UTF8Encoding]::new($false)
if(Test-Path -LiteralPath $OutputDirectory){throw 'Evidence exists'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$taskResult=@{target=$Target;started=[DateTime]::UtcNow.ToString('o');writes=0;serial_closed=$false}
$taskPort=$null;$taskId=60000
function Request([string]$op,$payload=@{}){
 $script:taskId++;$req=@{protocol_version=1;id=$script:taskId;op=$op;payload=$payload}
 [IO.File]::AppendAllText("$OutputDirectory/attempts.jsonl",(($req|ConvertTo-Json -Depth 30 -Compress)+"`n"),$utf8)
 if($op -in @('set_startup_usb_mode','save_config','reboot_to_saved_mode')){$taskResult.writes++}
 $taskPort.WriteLine(($req|ConvertTo-Json -Depth 30 -Compress));$r=$taskPort.ReadLine()|ConvertFrom-Json
 [IO.File]::AppendAllText("$OutputDirectory/cdc.jsonl",((@{time=[DateTime]::UtcNow.ToString('o');request=$req;response=$r}|ConvertTo-Json -Depth 65 -Compress)+"`n"),$utf8)
 if(!$r.ok -or $r.id -ne $script:taskId -or $r.protocol_version -ne 1){throw "Bad $op"};return $r
}
function Healthy {
 $s=Request 'get_status'
 if(!$s.result.config_ready -or $s.result.applying -or $s.result.apply_failed -or $s.result.maintenance -ne 'idle' -or $s.result.physical_mask -ne 0 -or $s.revision -ne $s.applied_revision -or $s.result.valid_slots -ne 3){throw 'Board not neutral/healthy or saved slots changed'}
 return $s
}
try {
 # Incident evidence is retained. Continued tests are authorized; fresh post-incident functional recovery, not electrical clearance, gates entry.
 if($Target -eq 'IO4'){
  $gate=Get-Content -LiteralPath "$PSScriptRoot/pc_board_2026-10-04/gp9-post-short-retry2.actual.json" -Encoding UTF8 -Raw|ConvertFrom-Json
  if(!$gate.passed -or $gate.serial -ne '5054316560DD729C' -or !$gate.forced_recovery -or $gate.revision -ne $gate.saved_revision){throw 'Fresh post-incident GP9 recovery must pass first'}
 }
 $ports=@(Get-CimInstance Win32_SerialPort|Where-Object PNPDeviceID -Like 'USB\VID_239A&PID_CAFE&MI_00\*')
 if($ports.Count -ne 1){throw 'Unique NORMAL CDC required'}
 $taskPort=[IO.Ports.SerialPort]::new($ports[0].DeviceID,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
 $taskPort.ReadTimeout=12000;$taskPort.WriteTimeout=5000;$taskPort.NewLine="`n";$taskPort.DtrEnable=$true;$taskPort.Open()
 $h=Request 'hello';$s=Healthy;$m=Request 'get_usb_mode';$d=Request 'get_device'
 if($h.result.serial -ne '5054316560DD729C' -or $h.result.firmware_version -ne '0.10.1' -or $h.result.usb_mode -ne 'NORMAL_NKRO' -or $d.result.active_profile -ne 4){throw 'Identity/profile mismatch'}
 $expectedSaved=if($Target -eq 'IO4'){'NORMAL_NKRO'}else{'IO4'}
 if($m.result.current -ne 'NORMAL_NKRO' -or $m.result.requested_startup -ne $expectedSaved -or $m.result.saved_startup -ne $expectedSaved -or $h.saved_revision -ne $h.revision -or $h.dirty -or $h.revision -ne $s.revision -or $m.revision -ne $s.revision -or $d.revision -ne $s.revision){throw 'Unexpected initial saved mode/revision'}
 if($Target -eq 'IO4' -and (!$m.result.forced_recovery -or $h.revision -ne $gate.revision)){throw 'Recovery preflight state changed; reread/review required'}
 if($Target -eq 'NORMAL_NKRO' -and !$m.result.forced_recovery){throw 'Return must be through physical recovery'}
 $r=Request 'set_startup_usb_mode' @{expected_revision=$s.revision;mode=$Target}
 $s=Healthy;$m=Request 'get_usb_mode'
 if($m.result.current -ne 'NORMAL_NKRO' -or $m.result.requested_startup -ne $Target){throw 'RAM mode preview mismatch'}
 $taskResult.ram_preview_kept_normal=$true
 $r=Request 'save_config' @{expected_revision=$s.revision};$s=Healthy;$m=Request 'get_usb_mode'
 if($r.saved_revision -ne $r.revision -or $r.dirty -or $s.result.save_result -ne 1 -or $m.result.saved_startup -ne $Target){throw 'Mode not saved'}
 $taskResult.saved_revision=$r.revision
 $r=Request 'reboot_to_saved_mode' @{expected_revision=$s.revision;target=$Target}
 if(!$r.result.reboot_ready){throw 'Reboot ACK not ready'}
 $taskResult.reboot_ack_received=$true
 Start-Sleep -Milliseconds 450
}catch {$taskResult.error=$_.Exception.Message;throw}
finally {
 if($taskPort){try {$taskPort.Close()}finally{$taskPort.Dispose()}}
 $taskResult.serial_closed=$true;$taskResult.finished=[DateTime]::UtcNow.ToString('o')
 [IO.File]::WriteAllText("$OutputDirectory/summary.json",($taskResult|ConvertTo-Json -Depth 10),$utf8)
}
