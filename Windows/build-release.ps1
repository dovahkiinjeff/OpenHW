[CmdletBinding()]
param(
    [string]$Python = 'python',
    [string]$VcpkgRoot,
    [string]$VcpkgInstalledRoot,
    [string]$SdkRoot,
    [string]$OutputDirectory,
    [switch]$SkipBootstrap,
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $VcpkgRoot) { $VcpkgRoot = Join-Path $repo '.deps\vcpkg' }
if (-not $VcpkgInstalledRoot) { $VcpkgInstalledRoot = Join-Path $repo '.deps\installed' }
if (-not $SdkRoot) { $SdkRoot = Join-Path $repo '.deps' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repo 'out\distribution' }
if ($VcpkgRoot -match '\s' -or $VcpkgInstalledRoot -match '\s') {
    throw 'FFmpeg dependencies need a path without spaces. Supply -VcpkgRoot and -VcpkgInstalledRoot if needed.'
}
function Run-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
if (-not $SkipBootstrap) { & (Join-Path $PSScriptRoot 'bootstrap.ps1') -VcpkgRoot $VcpkgRoot }
$venv = Join-Path $repo 'out\release-python'
$venvPython = Join-Path $venv 'Scripts\python.exe'
if (-not (Test-Path -LiteralPath $venvPython)) { Run-Checked $Python @('-m','venv',$venv) }
$localTemp = Join-Path $repo 'out\Temp'
New-Item -ItemType Directory -Force -Path $localTemp | Out-Null
$env:TEMP = $localTemp; $env:TMP = $localTemp
$env:PIP_CACHE_DIR = Join-Path $repo 'out\pip-cache'
Run-Checked $venvPython @('-m','pip','install','-r',(Join-Path $PSScriptRoot 'release-python-requirements.txt'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$version = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
if (-not $version) { throw 'Install Visual Studio with Desktop development with C++ and a Windows SDK.' }
$generator = if (([version]$version).Major -ge 18) {'Visual Studio 18 2026'} else {'Visual Studio 17 2022'}
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$ctest = (Get-Command ctest -ErrorAction Stop).Source
$normalBuild = Join-Path $repo 'out\build\release-no-diagnostics'
$diagnosticBuild = Join-Path $repo 'out\build\deploy-x64-clean'
foreach ($variant in @(@{Path=$normalBuild; Logging='OFF'},@{Path=$diagnosticBuild; Logging='ON'})) {
    Run-Checked $cmake @('-S',$repo,'-B',$variant.Path,'-G',$generator,'-A','x64',
        "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake",
        "-DVCPKG_INSTALLED_DIR=$VcpkgInstalledRoot",'-DVCPKG_TARGET_TRIPLET=x64-windows',
        "-DHW_STREAMLINE_ROOT=$SdkRoot\streamline","-DHW_FIDELITYFX_ROOT=$SdkRoot\fidelityfx","-DHW_XESS_ROOT=$SdkRoot\xess",
        "-DHW_ENABLE_DIAGNOSTICS=$($variant.Logging)",'-DBUILD_TESTING=ON')
    # Build every test executable before ctest, plus the import-only targets.
    Run-Checked $cmake @('--build',$variant.Path,'--config','Release','--parallel',
        '--','/v:minimal','/clp:ErrorsOnly;Summary')
    Run-Checked $cmake @('--build',$variant.Path,'--config','Release','--parallel','--target',
        'OpenHWSetupBootstrap','bc6h_encode','ship_texture_dds','remastered_asset_reader','OpenHWAssetCheck',
        '--','/v:minimal','/clp:ErrorsOnly;Summary')
    if (-not $SkipTests) { Run-Checked $ctest @('--test-dir',$variant.Path,'-C','Release','--output-on-failure') }
}
Push-Location $repo
try {
    if (-not $SkipTests) {
        Run-Checked $venvPython @('-m','unittest','discover','-s','tests','-p','test_release_*.py')
    }
    $tools = Join-Path $repo 'out\import-tools'
    New-Item -ItemType Directory -Force -Path $tools | Out-Null
    foreach ($name in @('bc6h_encode.exe','ship_texture_dds.exe','DirectXTex.dll')) {
        Copy-Item -LiteralPath (Join-Path $diagnosticBuild "Release\$name") -Destination $tools -Force
    }
    foreach ($name in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
        Copy-Item -LiteralPath (Join-Path $env:SystemRoot "System32\$name") -Destination $tools -Force
    }
    # A fresh staging directory prevents stale data from a previous build leaking in.
    $stage = Join-Path $repo ('out\release-stage-'+[guid]::NewGuid().ToString('N'))
    Run-Checked $venvPython @('tools\stage_release.py','--repo',$repo,'--profile','assets\Release',
        '--output',$stage,'--normal-build',"$normalBuild\Release",'--diagnostic-build',"$diagnosticBuild\Release",'--tools-dir',$tools)
    $frozen = Join-Path $repo 'out\release-installer-freeze'
    Run-Checked $venvPython @('tools\build_release_setup.py','--pyinstaller-modules',"$venv\Lib\site-packages",'--output',$frozen)
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    Run-Checked $venvPython @('tools\release_source_manifest.py',$repo)
    Run-Checked $venvPython @('tools\package_release_installer.py','--stage',$stage,
        '--frozen',"$frozen\OpenHW-Setup",'--bootstrap',"$diagnosticBuild\Release\OpenHWSetupBootstrap.exe",
        '--output',"$OutputDirectory\OpenHW-Setup.exe")
    Run-Checked $venvPython @('tools\audit_release_bundle.py','--installer',"$OutputDirectory\OpenHW-Setup.exe",'--source',$repo,
        '--output',"$OutputDirectory\Release-Audit.json")
    Write-Host "Built: $OutputDirectory\OpenHW-Setup.exe"
} finally { Pop-Location }
