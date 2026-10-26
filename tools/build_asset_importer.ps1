[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Python,
    [Parameter(Mandatory)][string]$RealEsrgan,
    [Parameter(Mandatory)][string]$SkyEncoder,
    [Parameter(Mandatory)][string]$TextureConverter,
    [string]$PyInstallerModules
)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output = Join-Path $repo 'out\asset-importer'
$arguments = @('-m','PyInstaller','--noconfirm','--clean','--onedir','--console',
    '--name','HomeworldAssetImporter','--distpath',$output,
    '--workpath',(Join-Path $repo 'out\importer-freeze-work'),
    '--specpath',(Join-Path $repo 'out\importer-freeze-work'),
    '--paths',$PSScriptRoot)
foreach ($module in @('extract_remastered_big','extract_vanilla_ship_textures',
    'classic_mesh_layout','ship_texture_pipeline','convert_ship_textures_to_dds',
    'translate_remastered_background','translate_remastered_scene','translate_remastered_shaders','inspect_remastered_scene','generate_mission_skies',
    'import_remastered_sky')) {
    $arguments += @('--hidden-import',$module)
}
$arguments += (Join-Path $PSScriptRoot 'import_local_assets.py')
$previousPythonPath = $env:PYTHONPATH
try {
    if ($PyInstallerModules) { $env:PYTHONPATH = [IO.Path]::GetFullPath($PyInstallerModules) }
    & $Python @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Importer freeze failed' }
} finally { $env:PYTHONPATH = $previousPythonPath }
$tools = Join-Path $output 'HomeworldAssetImporter\Tools'
New-Item -ItemType Directory -Path $tools -Force | Out-Null
Copy-Item -LiteralPath $SkyEncoder -Destination (Join-Path $tools 'bc6h_encode.exe')
Copy-Item -LiteralPath $TextureConverter -Destination (Join-Path $tools 'ship_texture_dds.exe')
Copy-Item -LiteralPath (Join-Path (Split-Path -Parent $TextureConverter) 'DirectXTex.dll') -Destination $tools
# App-local VC runtimes keep conversion tools independent of a machine-wide
# redistributable installation. The build machine must have the release DLLs.
foreach ($name in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot "System32\$name") -Destination $tools
}
$ai = Join-Path $tools 'realesrgan'
New-Item -ItemType Directory -Path (Join-Path $ai 'models') -Force | Out-Null
Copy-Item -LiteralPath $RealEsrgan -Destination (Join-Path $ai 'realesrgan-ncnn-vulkan.exe')
foreach ($name in @('msvcp140.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    Copy-Item -LiteralPath (Join-Path $tools $name) -Destination $ai
}
$sourceAI = Split-Path -Parent $RealEsrgan
foreach ($name in @('realesrgan-x4plus.param','realesrgan-x4plus.bin')) {
    Copy-Item -LiteralPath (Join-Path $sourceAI "models\$name") -Destination (Join-Path $ai "models\$name")
}
foreach ($name in @('vcomp140.dll','LICENSE','LICENSE.txt','README_windows.md')) {
    $source = Join-Path $sourceAI $name
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination (Join-Path $ai $name) }
}
$notices = Join-Path $output 'HomeworldAssetImporter\ThirdPartyNotices'
New-Item -ItemType Directory -Path $notices -Force | Out-Null
$pythonLicense = Join-Path (Split-Path -Parent $Python) 'LICENSE.txt'
if (Test-Path -LiteralPath $pythonLicense) {
    Copy-Item -LiteralPath $pythonLicense -Destination (Join-Path $notices 'Python-LICENSE.txt')
}
$licenseInventory = & $Python -c 'import importlib.metadata as m,json; print(json.dumps([{ "name":n,"relative":str(f),"path":str(m.distribution(n).locate_file(f))} for n in ("numpy","pillow") for f in m.distribution(n).files if "license" in str(f).lower() or "copying" in str(f).lower()]))'
if ($LASTEXITCODE -ne 0) { throw 'Dependency license inventory failed' }
foreach ($notice in ($licenseInventory | ConvertFrom-Json)) {
    $destination = Join-Path $notices ($notice.name + '\' + $notice.relative)
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $notice.path -Destination $destination
}
if ($PyInstallerModules) {
    foreach ($name in @('COPYING.txt','LICENSE')) {
        $notice = Join-Path $PyInstallerModules "PyInstaller\$name"
        if (Test-Path -LiteralPath $notice) {
            Copy-Item -LiteralPath $notice -Destination (Join-Path $notices "PyInstaller-$name")
        }
    }
}
Write-Output "Importer package: $output\HomeworldAssetImporter"
