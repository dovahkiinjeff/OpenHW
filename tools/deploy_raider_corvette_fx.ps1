param([string]$Release='F:\SteamLibrary\steamapps\common\Homeworld\Homeworld1Classic\RTX\RTX Deployed\Release')
$ErrorActionPreference='Stop'
if(Get-Process HomeworldModern -ErrorAction SilentlyContinue) { throw 'Close the game before deployment.' }
$repo=(Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$targetRoot=(Resolve-Path -LiteralPath $Release).Path
$files=@(
    @{Source='out\hwrm-weapon-fx\runtime-raiders\Kushan.rfx';Target='RemasteredFX\Kushan.rfx'},
    @{Source='out\build\deploy-x64-clean\Release\HomeworldModern.pdb';Target='HomeworldModern.pdb'},
    @{Source='out\build\deploy-x64-clean\Release\HomeworldModern.exe';Target='HomeworldModern.exe'}
)
$backupRoot=Join-Path $repo ('out\deployment-backups\pre-raider-corvette-fx-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
foreach($file in $files) {
    $source=Join-Path $repo $file.Source
    $target=[IO.Path]::GetFullPath((Join-Path $targetRoot $file.Target))
    if(-not $target.StartsWith($targetRoot+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Target escaped release.'}
    if(-not (Test-Path -LiteralPath $source -PathType Leaf)) {throw "Missing source: $source"}
    if(Test-Path -LiteralPath $target -PathType Leaf) {
        $backup=Join-Path $backupRoot $file.Target
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $backup) | Out-Null
        Copy-Item -LiteralPath $target -Destination $backup
    }
}
if(Get-Process HomeworldModern -ErrorAction SilentlyContinue) {throw 'Game started; cancelled after backup.'}
foreach($file in $files) {
    $source=Join-Path $repo $file.Source;$target=Join-Path $targetRoot $file.Target
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
    Copy-Item -LiteralPath $source -Destination $target -Force
    if((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $target).Hash) {throw "Hash mismatch: $target"}
}
Write-Output "Deployed and verified 3 files. Backup: $backupRoot"
Get-FileHash -LiteralPath (Join-Path $targetRoot 'HomeworldModern.exe')
