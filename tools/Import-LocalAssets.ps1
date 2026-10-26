[CmdletBinding()]
param(
    [string]$Remastered,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$runtime = Split-Path -Parent $PSScriptRoot
$importer = Join-Path $runtime 'AssetImporter\HomeworldAssetImporter.exe'
if (-not (Test-Path -LiteralPath $importer)) { throw 'AssetImporter is not packaged beside this installation.' }
$cache = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Homeworld Modern\ImportedAssets'
$arguments = @('--only','remastered','--cache',$cache,'--tools',(Join-Path $runtime 'AssetImporter\Tools'),'--runtime',$runtime)
if ($Remastered) { $arguments += @('--remastered',$Remastered) }
if ($Force) { $arguments += '--force' }
& $importer @arguments
if ($LASTEXITCODE -ne 0) { throw 'Asset import failed; see the preceding error. Previous valid cache is unchanged.' }
