param(
    [string]$Repository = (Split-Path -Parent $PSScriptRoot),
    [string]$Release = 'F:\SteamLibrary\steamapps\common\Homeworld\Homeworld1Classic\RTX\RTX Deployed\Release'
)
$ErrorActionPreference = 'Stop'
if (Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Close the game before deployment.' }
$repoRoot = (Resolve-Path -LiteralPath $Repository).Path
$releaseRoot = (Resolve-Path -LiteralPath $Release).Path
$cacheRoot = (Resolve-Path -LiteralPath (Join-Path $repoRoot 'out\hwrm-taiidan-native\package-cache\v1')).Path
$plan = Get-Content -LiteralPath (Join-Path $repoRoot 'out\hwrm-taiidan-native\plan.json') -Raw | ConvertFrom-Json
$report = Get-Content -LiteralPath (Join-Path $cacheRoot 'manifest.json') -Raw | ConvertFrom-Json
if (@($plan.ships).Count -ne 29 -or @($report.ships).Count -ne 29) { throw 'Incomplete Taiidan fleet.' }
$files = @()
foreach ($ship in $plan.ships) {
    if ($ship.classic_race -ne 'R2' -or $ship.classic_type -notmatch '^[A-Za-z0-9_]+$' -or $ship.source -notmatch '^tai_[a-z0-9_]+$') {
        throw 'Unsafe Taiidan identity.'
    }
    $relative = Join-Path 'R2' (Join-Path $ship.classic_type 'RMNative')
    $folder = Join-Path $cacheRoot $relative
    $manifest = Get-Content -LiteralPath (Join-Path $folder 'visual.rmnative')
    if ($manifest[0] -ne 'HW_RM_R2_VISUAL_1' -or $manifest[1] -ne ('type Taiidan_'+$ship.classic_type)) { throw 'Package identity mismatch.' }
    foreach ($file in Get-ChildItem -LiteralPath $folder -File) {
        if ($file.Extension -notin @('.hod','.mad','.dds','.lif','.rmnative')) { throw 'Unexpected Taiidan file.' }
        $files += [pscustomobject]@{ Source=$file.FullName; Relative=(Join-Path $relative $file.Name) }
    }
}
foreach ($name in @('HomeworldModern.exe','HomeworldModern.pdb')) {
    $files += [pscustomobject]@{ Source=(Join-Path $repoRoot ('out\build\deploy-x64-clean\Release\'+$name)); Relative=$name }
}
$backupRoot = Join-Path $repoRoot ('out\deployment-backups\pre-native-taiidan-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
# Validate the entire allowlisted set before changing the release. No saves,
# mission authoring, other races, original archives or gameplay files are copied.
foreach ($file in $files) {
    $target = [IO.Path]::GetFullPath((Join-Path $releaseRoot $file.Relative))
    if (-not $target.StartsWith($releaseRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Target escaped release.' }
    if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) { throw 'Missing source.' }
    $file | Add-Member -NotePropertyName Hash -NotePropertyValue ((Get-FileHash -LiteralPath $file.Source -Algorithm SHA256).Hash)
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        $backup = Join-Path $backupRoot $file.Relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $backup) | Out-Null
        Copy-Item -LiteralPath $target -Destination $backup
    }
}
if (Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Game started; deployment cancelled.' }
foreach ($file in $files) {
    $target = Join-Path $releaseRoot $file.Relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $file.Source -Destination $target -Force
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $file.Hash) { throw "Deployment hash mismatch: $target" }
}
$files | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $backupRoot 'deployed-files.json')
Write-Output "Deployed and hash-verified $($files.Count) files. Backup: $backupRoot"
Get-FileHash -LiteralPath (Join-Path $releaseRoot 'HomeworldModern.exe') -Algorithm SHA256
