param(
    [string]$Repository = (Split-Path -Parent $PSScriptRoot),
    [string]$Release = 'F:\SteamLibrary\steamapps\common\Homeworld\Homeworld1Classic\RTX\RTX Deployed\Release'
)
$ErrorActionPreference = 'Stop'
if (Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Close the game before deployment.' }
$repoRoot = (Resolve-Path -LiteralPath $Repository).Path
$releaseRoot = (Resolve-Path -LiteralPath $Release).Path
$workRoot = Join-Path $repoRoot 'out\classic-fx-256-2048'
$records = Get-Content -LiteralPath (Join-Path $workRoot 'weapon-fx-manifest.json') -Raw | ConvertFrom-Json
if ($records.Count -ne 425) { throw 'Incomplete Classic FX texture set.' }
$files = @()
foreach ($record in $records) {
    if (($record.output_size | Measure-Object -Minimum).Minimum -lt 256 -or
        ($record.output_size | Measure-Object -Maximum).Maximum -gt 2048 -or $record.normal_generated) {
        throw 'FX texture violates the resolution/no-normal policy.'
    }
    $relative = $record.dds.Replace('/','\')
    if (-not $relative.StartsWith('etg\',[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetExtension($relative) -ne '.dds') { throw 'Unexpected FX path.' }
    $files += [pscustomobject]@{Relative=$relative;Source=(Join-Path (Join-Path $workRoot 'dds') $relative);Hash=$record.sha256}
}
foreach ($name in @('HomeworldModern.exe','HomeworldModern.pdb')) {
    $source = Join-Path $repoRoot ('out\build\deploy-x64-clean\Release\'+$name)
    $files += [pscustomobject]@{Relative=$name;Source=$source;Hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash}
}
# Validate every source and resolved destination before changing the release.
foreach ($file in $files) {
    $target = [IO.Path]::GetFullPath((Join-Path $releaseRoot $file.Relative))
    if (-not $target.StartsWith($releaseRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe deployment target.' }
    if ((Get-FileHash -LiteralPath $file.Source -Algorithm SHA256).Hash -ne $file.Hash) { throw 'Source hash mismatch.' }
}
$backupRoot = Join-Path $repoRoot ('out\deployment-backups\pre-classic-fx-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backupRoot | Out-Null
foreach ($file in $files) {
    $target = Join-Path $releaseRoot $file.Relative
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
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $file.Hash) { throw 'Deployed hash mismatch.' }
}
Write-Output "Deployed and verified Classic FX executable + 425 DDS textures. Backup: $backupRoot"
