param([Parameter(Mandatory=$true)][string]$EvidenceRoot)
$ErrorActionPreference='Stop'
$a=Get-Content -LiteralPath "$EvidenceRoot/initial/original-profiles.json" -Raw|ConvertFrom-Json
$b=Get-Content -LiteralPath "$EvidenceRoot/flash-normal/final/original-profiles.json" -Raw|ConvertFrom-Json
$tokens=$null;$errors=$null
$null=[Management.Automation.Language.Parser]::ParseFile("$PSScriptRoot/test_flash_normal_v2.ps1",[ref]$tokens,[ref]$errors)
$equal=($a|ConvertTo-Json -Depth 80 -Compress) -eq ($b|ConvertTo-Json -Depth 80 -Compress)
$r=@{powershell_version=$PSVersionTable.PSVersion.ToString();normalized_profiles_equal=$equal;v2_syntax_errors=@($errors|ForEach-Object Message);device_requests=0}
$out="$EvidenceRoot/flash-normal/v2-offline-check.json"
if(Test-Path -LiteralPath $out){throw 'Refusing to overwrite check'}
[IO.File]::WriteAllText($out,($r|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
if(!$equal -or $errors.Count){throw 'Offline tool check failed'}
$r|ConvertTo-Json -Compress
