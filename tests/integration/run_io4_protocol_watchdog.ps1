param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$PreflightOnly)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Evidence exists'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$taskPython=(Get-Command python).Source;$taskScript=(Resolve-Path "$PSScriptRoot/test_io4_protocol_actual_v2.py").Path
Copy-Item -LiteralPath $taskScript -Destination "$OutputDirectory/test_io4_protocol_actual_v2-executed.py"
$env:PYTHONIOENCODING='utf-8'
$taskArgs=@($taskScript,'--serial','5054316560DD729C','--output',"$OutputDirectory/result.json")
if($PreflightOnly){$taskArgs+='--preflight-only'}
$taskRun=@{started=[DateTime]::UtcNow.ToString('o');timeout_seconds=45;script_sha256=(Get-FileHash $taskScript -Algorithm SHA256).Hash;preflight_only=[bool]$PreflightOnly}
$taskChild=Start-Process -FilePath $taskPython -ArgumentList $taskArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$OutputDirectory/stdout.log" -RedirectStandardError "$OutputDirectory/stderr.log"
$taskRun.child_pid=$taskChild.Id;$taskChildStart=$taskChild.StartTime
$taskRun|ConvertTo-Json|Set-Content "$OutputDirectory/watchdog.json" -Encoding UTF8
if(!$taskChild.WaitForExit(45000)){
 $taskCurrent=Get-Process -Id $taskChild.Id -ErrorAction SilentlyContinue
 if($taskCurrent -and $taskCurrent.StartTime -eq $taskChildStart -and $taskCurrent.Path -eq $taskPython){Stop-Process -Id $taskCurrent.Id -Force}
 $taskRun.timeout_killed=$true;$taskRun.independent_readback_or_power_recovery_required=$true
}else{$taskChild.Refresh();$taskRun.exit_code=$taskChild.ExitCode}
$taskRun.finished=[DateTime]::UtcNow.ToString('o');$taskRun|ConvertTo-Json|Set-Content "$OutputDirectory/watchdog.json" -Encoding UTF8
