$ErrorActionPreference='Stop'
$taskRoot='C:/Users/admin/Documents/FluxRail'
$taskLaunch="$taskRoot/tests/integration/hid_etw_launch_2026-10-04_retry1"
$taskOutput="$taskRoot/tests/integration/hid_etw_run_2026-10-04_retry1"
$taskResult=[ordered]@{started=[DateTime]::UtcNow.ToString('o');process_id=$PID;capture_directory=$taskOutput;exit_code=$null}
$taskPrincipal=[Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
$taskResult.administrator=$taskPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$taskResult.capture_tool_sha256=(Get-FileHash -LiteralPath "$taskRoot/tests/integration/capture_hid_etw_v2.ps1" -Algorithm SHA256).Hash
$taskUtf8=[Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText("$taskLaunch/started.json",($taskResult|ConvertTo-Json -Depth 6),$taskUtf8)
try {
 if(!$taskResult.administrator){throw 'UAC did not supply an administrator token'}
 Set-Location -LiteralPath $taskRoot
 & "$taskRoot/tests/integration/capture_hid_etw_v2.ps1" -Port COM29 -Serial 5054316560DD729C -Version 0.10.1 -OutputDirectory $taskOutput -Python 'C:/Users/admin/AppData/Local/Programs/Python/Python312/python.exe' *>> "$taskLaunch/capture-output.log"
 $taskResult.exit_code=0
} catch {
 $taskResult.exit_code=1
 $taskResult.error=$_.Exception.Message
 $taskResult.error_details=($_|Out-String)
} finally {
 $taskResult.finished=[DateTime]::UtcNow.ToString('o')
 [IO.File]::WriteAllText("$taskLaunch/completed.json",($taskResult|ConvertTo-Json -Depth 8),$taskUtf8)
}
exit $taskResult.exit_code
