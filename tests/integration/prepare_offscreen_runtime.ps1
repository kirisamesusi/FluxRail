$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$taskRoot=Join-Path $PSScriptRoot 'build/pc_browser_runtime'
$taskZip=Join-Path $taskRoot 'electron-v44.5.1-win32-x64.zip'
$taskResult=Join-Path $taskRoot 'runtime-prepare-result.json'
$taskReport=@{started=[DateTime]::UtcNow.ToString('o');source='https://github.com/electron/electron/releases/download/v44.5.1/electron-v44.5.1-win32-x64.zip';pid=$PID;hardware_requests=0}
try {
 Invoke-WebRequest -Uri $taskReport.source -OutFile $taskZip -Resume -TimeoutSec 60
 $taskChecks=Get-Content -LiteralPath (Join-Path $taskRoot 'node_modules/electron/checksums.json') -Raw|ConvertFrom-Json
 $taskDigest=(Get-FileHash -LiteralPath $taskZip -Algorithm SHA256).Hash.ToLower()
 $taskReport.sha256=$taskDigest
 if($taskDigest -ne $taskChecks.'electron-v44.5.1-win32-x64.zip'){throw 'Official npm package checksum mismatch'}
 $taskReport.verified_official_checksum=$true
 $taskReport.length=(Get-Item -LiteralPath $taskZip).Length
 $taskDest=Join-Path $taskRoot 'node_modules/electron/dist'
 if(Test-Path -LiteralPath $taskDest){throw 'Extraction destination already exists'}
 Expand-Archive -LiteralPath $taskZip -DestinationPath $taskDest
 [IO.File]::WriteAllText((Join-Path $taskRoot 'node_modules/electron/path.txt'),'electron.exe')
 $taskReport.exe=Join-Path $taskDest 'electron.exe'
 $taskReport.complete=$true
} catch {$taskReport.complete=$false;$taskReport.error=$_.Exception.Message}
finally {$taskReport.finished=[DateTime]::UtcNow.ToString('o');[IO.File]::WriteAllText($taskResult,($taskReport|ConvertTo-Json -Depth 5))}
if(!$taskReport.complete){exit 1}
