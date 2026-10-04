param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$utf8=[Text.UTF8Encoding]::new($false)
if(Test-Path -LiteralPath $OutputDirectory){throw 'Evidence exists'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$result=@{started=[DateTime]::UtcNow.ToString('o');config_flash_writes=0;enter_requests=0;serial_closed=$false}
$p=$null;$script:id=71000
function Request([string]$op,$payload=@{}){
 $script:id++;$q=@{protocol_version=1;id=$script:id;op=$op;payload=$payload}
 [IO.File]::AppendAllText("$OutputDirectory/attempts.jsonl",(($q|ConvertTo-Json -Depth 20 -Compress)+"`n"),$utf8)
 if($op -eq 'enter_bootloader'){$result.enter_requests++}
 $p.WriteLine(($q|ConvertTo-Json -Depth 20 -Compress));$r=$p.ReadLine()|ConvertFrom-Json
 [IO.File]::AppendAllText("$OutputDirectory/cdc.jsonl",((@{time=[DateTime]::UtcNow.ToString('o');request=$q;response=$r}|ConvertTo-Json -Depth 60 -Compress)+"`n"),$utf8)
 if(!$r.ok -or $r.id -ne $script:id -or $r.protocol_version -ne 1){throw "Bad $op"};return $r
}
try {
 $rom=@(Get-PnpDevice -PresentOnly|Where-Object InstanceId -Like 'USB\VID_2E8A&PID_0003\*')
 if($rom.Count){throw 'Existing ROM device; target correlation ambiguous'}
 $ports=@(Get-CimInstance Win32_SerialPort|Where-Object PNPDeviceID -Like 'USB\VID_239A&PID_CAFE&MI_00\*')
 if($ports.Count -ne 1){throw 'Unique normal CDC required'}
 $p=[IO.Ports.SerialPort]::new($ports[0].DeviceID,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
 $p.ReadTimeout=12000;$p.WriteTimeout=5000;$p.NewLine="`n";$p.DtrEnable=$true;$p.Open()
 $h=Request 'hello';$s=Request 'get_status';$m=Request 'get_usb_mode';$d=Request 'get_device'
 if($h.result.serial -ne '5054316560DD729C' -or $h.result.firmware_version -ne '0.10.1' -or !$h.result.capabilities.bootloader_command){throw 'Identity/capability mismatch'}
 foreach($r in @($h,$s,$m,$d)){if($r.revision -ne 9 -or $r.applied_revision -ne 9 -or $r.saved_revision -ne 9 -or $r.dirty){throw 'Unexpected revision/save state'}}
 if(!$s.result.config_ready -or $s.result.applying -or $s.result.apply_failed -or $s.result.maintenance -ne 'idle' -or $s.result.physical_mask -ne 0 -or $s.result.valid_slots -ne 3 -or $d.result.active_profile -ne 4){throw 'Not healthy neutral original configuration'}
 if($m.result.current -ne 'NORMAL_NKRO' -or $m.result.requested_startup -ne 'NORMAL_NKRO' -or $m.result.saved_startup -ne 'NORMAL_NKRO' -or $m.result.forced_recovery){throw 'Unexpected mode'}
 $r=Request 'enter_bootloader' @{expected_revision=9}
 if(!$r.result.reboot_ready){throw 'ROM ACK not ready'}
 $result.ack_received=$true
 Start-Sleep -Milliseconds 450
}catch{$result.error=$_.Exception.Message;throw}
finally{
 if($p){try{$p.Close()}finally{$p.Dispose()}}
 $result.serial_closed=$true;$result.finished=[DateTime]::UtcNow.ToString('o')
 [IO.File]::WriteAllText("$OutputDirectory/summary.json",($result|ConvertTo-Json -Depth 12),$utf8)
}
Start-Sleep -Milliseconds 1500
$nodes=@(Get-PnpDevice -PresentOnly|Where-Object {$_.InstanceId -Like 'USB\VID_2E8A&PID_0003\*' -or $_.InstanceId -Like 'USB\VID_239A&PID_CAFE\*'}|Select-Object Status,Class,FriendlyName,InstanceId)
[IO.File]::WriteAllText("$OutputDirectory/pnp-after.json",(ConvertTo-Json -InputObject $nodes -Depth 10),$utf8)
Write-Output (ConvertTo-Json -InputObject $nodes -Compress)
