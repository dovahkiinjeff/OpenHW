param(
    [string]$Repository = (Split-Path -Parent $PSScriptRoot),
    [string]$Release = 'F:\SteamLibrary\steamapps\common\Homeworld\Homeworld1Classic\RTX\RTX Deployed\Release'
)
$ErrorActionPreference = 'Stop'
if (Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Close the game before deployment.' }
$repoRoot = (Resolve-Path -LiteralPath $Repository).Path
$releaseRoot = (Resolve-Path -LiteralPath $Release).Path
$cacheRoot = (Resolve-Path -LiteralPath (Join-Path $repoRoot 'out\hwrm-raiders-derelicts-native\package-cache\v1')).Path
$report = Get-Content -LiteralPath (Join-Path $repoRoot 'out\hwrm-raiders-derelicts-native\manifest.json') -Raw | ConvertFrom-Json
if (@($report.packages).Count -ne 46 -or @($report.packages | Where-Object race -eq 'P1').Count -ne 5) {
    throw 'Incomplete Raider/derelict package set.'
}
$backupRoot = Join-Path $repoRoot ('out\deployment-backups\pre-native-raiders-derelicts-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
$files = @()
foreach ($package in $report.packages) {
    if ($package.race -notin @('P1','Derelicts') -or $package.type -notmatch '^[A-Za-z0-9_]+$') { throw 'Unsafe package identity.' }
    $relative = Join-Path $package.race (Join-Path $package.type 'RMNative')
    $folder = Join-Path $cacheRoot $relative
    foreach ($file in Get-ChildItem -LiteralPath $folder -File) {
        if ($file.Extension -notin @('.hod','.mad','.dds','.lif','.rmnative')) { throw 'Unexpected deployment asset.' }
        $files += [pscustomobject]@{ Source=$file.FullName; Relative=(Join-Path $relative $file.Name) }
    }
}
foreach ($name in @('HomeworldModern.exe','HomeworldModern.pdb')) {
    $files += [pscustomobject]@{ Source=(Join-Path $repoRoot ('out\build\deploy-x64-clean\Release\'+$name)); Relative=$name }
}
# Only explicit package assets and the executable/symbols are overlaid. Existing
# missions, saves, settings, textures for other races and unmatched models stay.
foreach ($file in $files) {
    $target = [IO.Path]::GetFullPath((Join-Path $releaseRoot $file.Relative))
    if (-not $target.StartsWith($releaseRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Deployment target escaped release.' }
    if (-not (Test-Path -LiteralPath $file.Source -PathType Leaf)) { throw 'Missing deployment source.' }
    if (Test-Path -LiteralPath $target -PathType Leaf) {
        $backup = Join-Path $backupRoot $file.Relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $backup) | Out-Null
        Copy-Item -LiteralPath $target -Destination $backup
    }
}
if (Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Game started; deployment cancelled after backup.' }
foreach ($file in $files) {
    $target = Join-Path $releaseRoot $file.Relative
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $file.Source -Destination $target -Force
    $expected = (Get-FileHash -LiteralPath $file.Source -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $expected) { throw "Deployment hash mismatch: $target" }
}
Write-Output "Deployed and hash-verified $($files.Count) files. Backup: $backupRoot"
Get-FileHash -LiteralPath (Join-Path $releaseRoot 'HomeworldModern.exe') -Algorithm SHA256
