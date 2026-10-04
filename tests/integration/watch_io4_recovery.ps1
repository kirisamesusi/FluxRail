param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Evidence exists'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$taskReport=@{started=[DateTime]::UtcNow.ToString('o');pid=$PID;events=@();device_writes=0};$taskStop=[DateTime]::UtcNow.AddMinutes(5);$taskAbsent=$false;$taskLast=$null
try {
 while([DateTime]::UtcNow -lt $taskStop){
  $taskIo4=[bool](Get-PnpDevice -PresentOnly -InstanceId 'USB\VID_0CA3&PID_0021\5054316560DD729C' -ErrorAction SilentlyContinue)
  $taskNormal=[bool](Get-PnpDevice -PresentOnly -InstanceId 'USB\VID_239A&PID_CAFE\5054316560DD729C' -ErrorAction SilentlyContinue)
  $taskPresent=$taskIo4 -or $taskNormal
  if($taskPresent -ne $taskLast){$taskReport.events+=@{present=$taskPresent;time=[DateTime]::UtcNow.ToString('o')};$taskLast=$taskPresent; $taskReport|ConvertTo-Json -Depth 8|Set-Content "$OutputDirectory/monitor.json" -Encoding UTF8}
  if(!$taskPresent){$taskAbsent=$true}
  if($taskAbsent -and $taskNormal){
   Start-Sleep -Milliseconds 350
   & "$PSScriptRoot/prepare_board_baseline.ps1" -Port COM29 -Serial 5054316560DD729C -Version 0.10.1 -OutputDirectory "$OutputDirectory/held-read"
   $taskReport.read_completed=$true;break
  }
  Start-Sleep -Milliseconds 200
 }
}catch {$taskReport.error=$_.Exception.Message}
finally {$taskReport.finished=[DateTime]::UtcNow.ToString('o');$taskReport|ConvertTo-Json -Depth 8|Set-Content "$OutputDirectory/monitor.json" -Encoding UTF8}
