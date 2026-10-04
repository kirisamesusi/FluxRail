param([Parameter(Mandatory=$true)][string]$OutputDirectory,[int]$TimeoutSeconds=240)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Evidence exists'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$taskReport=@{pid=$PID;started=[DateTime]::UtcNow.ToString('o');target='USB\VID_0CA3&PID_0021\5054316560DD729C';events=@();readonly=$true}
$taskStop=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$taskLast=$null;$taskAbsent=$false
try {
 while([DateTime]::UtcNow -lt $taskStop){
  $taskPresent=[bool](Get-PnpDevice -PresentOnly -InstanceId $taskReport.target -ErrorAction SilentlyContinue)
  if($taskLast -ne $taskPresent){
   $taskReport.events+=@{present=$taskPresent;utc=[DateTime]::UtcNow.ToString('o')}
   [IO.File]::WriteAllText("$OutputDirectory/summary.json",($taskReport|ConvertTo-Json -Depth 8))
   $taskLast=$taskPresent
  }
  if(!$taskPresent){$taskAbsent=$true}
  if($taskAbsent -and $taskPresent){$taskReport.disconnect_and_reconnect_observed=$true;break}
  Start-Sleep -Milliseconds 300
 }
}catch {$taskReport.error=$_.Exception.Message}
finally {$taskReport.finished=[DateTime]::UtcNow.ToString('o');[IO.File]::WriteAllText("$OutputDirectory/summary.json",($taskReport|ConvertTo-Json -Depth 8))}
