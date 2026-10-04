param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$PreflightOnly)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Evidence exists'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$taskPython=(Get-Command python).Source;$taskScript=(Resolve-Path "$PSScriptRoot/test_io4_protocol_diagnostic.py").Path
Copy-Item -LiteralPath $taskScript -Destination "$OutputDirectory/test_io4_protocol_diagnostic-executed.py"
$env:PYTHONIOENCODING='utf-8'
$taskArgs=@($taskScript,'--serial','5054316560DD729C','--output',"$OutputDirectory/result.json")
if($PreflightOnly){$taskArgs+='--preflight-only'}
$taskRun=@{started=[DateTime]::UtcNow.ToString('o');timeout_seconds=45;script_sha256=(Get-FileHash $taskScript -Algorithm SHA256).Hash;preflight_only=[bool]$PreflightOnly}
$taskChild=Start-Process -FilePath $taskPython -ArgumentList $taskArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$OutputDirectory/stdout.log" -RedirectStandardError "$OutputDirectory/stderr.log"
$taskRun.child_pid=$taskChild.Id;$taskChildStart=$taskChild.StartTime
$taskCachedHandle=$taskChild.Handle
$taskRun.launcher_sha256=(Get-FileHash $PSCommandPath -Algorithm SHA256).Hash
$taskRun|ConvertTo-Json|Set-Content "$OutputDirectory/watchdog.json" -Encoding UTF8
if(!$taskChild.WaitForExit(45000)){
 $taskRun.deadline_elapsed=$true;$taskRun.kill_attempted=$false
 $taskCurrent=Get-Process -Id $taskChild.Id -ErrorAction SilentlyContinue
 if($taskCurrent -and $taskCurrent.StartTime -eq $taskChildStart -and $taskCurrent.Path -eq $taskPython){$taskRun.kill_attempted=$true;Stop-Process -Id $taskCurrent.Id -Force;$taskRun.kill_completed=$taskChild.WaitForExit(1000)}
 $taskRun.independent_readback_or_power_recovery_required=$true
}
if($taskChild.HasExited){$taskRun.exit_code=$taskChild.ExitCode;$taskRun.exit_code_available=$null -ne $taskRun.exit_code}
$taskRun.finished=[DateTime]::UtcNow.ToString('o');$taskRun|ConvertTo-Json|Set-Content "$OutputDirectory/watchdog.json" -Encoding UTF8
