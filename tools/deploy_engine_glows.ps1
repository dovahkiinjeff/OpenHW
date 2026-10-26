param(
    [string]$Repository = (Split-Path -Parent $PSScriptRoot),
    [string]$Release = 'F:\SteamLibrary\steamapps\common\Homeworld\Homeworld1Classic\RTX\RTX Deployed\Release'
)
$ErrorActionPreference = 'Stop'
if (Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Close the game before deployment.' }
$repoRoot = (Resolve-Path -LiteralPath $Repository).Path
$releaseRoot = (Resolve-Path -LiteralPath $Release).Path
$sourceRoot = Join-Path $repoRoot 'out\engine-glow-subdivision\runtime\misc\engineglows'
$names = @()
foreach ($prefix in @('engine_glow0','engine_glow_frigate0','r1moship','r2moship','p1moship')) {
    foreach ($index in 0..3) { $names += ($prefix+$index+'.geo') }
}
$backupRoot = Join-Path $repoRoot ('out\deployment-backups\pre-engine-glow-subdivision-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$files = @()
foreach ($name in $names) {
    $source = Join-Path $sourceRoot $name
    $target = [IO.Path]::GetFullPath((Join-Path $releaseRoot ('misc\EngineGlows\'+$name)))
    if (-not $target.StartsWith($releaseRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe deployment target.' }
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw 'Missing subdivided mesh.' }
    if (Test-Path -LiteralPath ([IO.Path]::ChangeExtension($target,'.peo'))) { throw 'Paged mesh would shadow the GEO override.' }
    $files += [pscustomobject]@{Source=$source;Target=$target;Name=$name;Hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash;PreviousOverride=(Test-Path -LiteralPath $target)}
}
New-Item -ItemType Directory -Path $backupRoot | Out-Null
foreach ($file in $files) {
    if ($file.PreviousOverride) { Copy-Item -LiteralPath $file.Target -Destination (Join-Path $backupRoot $file.Name) }
}
if (Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Game started; deployment cancelled.' }
New-Item -ItemType Directory -Force -Path (Join-Path $releaseRoot 'misc\EngineGlows') | Out-Null
foreach ($file in $files) {
    Copy-Item -LiteralPath $file.Source -Destination $file.Target -Force
    if ((Get-FileHash -LiteralPath $file.Target -Algorithm SHA256).Hash -ne $file.Hash) { throw 'Mesh deployment hash mismatch.' }
}
$files | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $backupRoot 'deployed-files.json')
Write-Output "Deployed and verified 20 glow meshes only. Backup: $backupRoot"
