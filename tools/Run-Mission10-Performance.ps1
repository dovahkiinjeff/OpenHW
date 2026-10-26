[CmdletBinding()]
param([switch]$CheckOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Install beside Run-Camera-Stutter-Diagnostics.ps1 in the live Dev directory.
# Reuse the validated recorder and current binary; do not change game settings.
$launcher = Join-Path $PSScriptRoot 'Run-Camera-Stutter-Diagnostics.ps1'
$gameRoot = Split-Path -Parent $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'PerformanceCandidate\manifest.json') -Raw | ConvertFrom-Json
& $launcher -CheckOnly
foreach ($runtime in $manifest.runtimeSHA256.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (Join-Path $gameRoot $runtime.Name)).Hash -ne $runtime.Value) {
        throw "Runtime differs from the recorded build: $($runtime.Name)"
    }
}
foreach ($path in @(
    (Join-Path $env:SystemRoot 'System32\wpr.exe'),
    'C:\Program Files (x86)\RivaTuner Statistics Server\Plugins\Client\PresentMonDataProvider\PresentMon-1.9.2-x64.exe'
)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing recorder: $path" }
}
if ($CheckOnly) {
    Write-Output 'Mission 10 diagnostics verified. No game or recorder started; no settings changed.'
    return
}

# Image readbacks and cloud counterfactual rendering belong to visual diagnosis,
# not performance measurement. Remove inherited visual-capture switches only
# from this launcher process. The current game and saved config are untouched.
$captureVariables = @('HW_TEMPORAL_CAPTURE', 'HW_FG_ZOOM_CAPTURE',
    'HW_FG_ZOOM_CAPTURE_DIR', 'HW_FG_ZOOM_CAPTURE_ARM_FILE', 'HW_FSR_FG_DEBUG')
$savedEnvironment = @{}
foreach ($name in $captureVariables) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
    [Environment]::SetEnvironmentVariable($name, $null, 'Process')
}
try {
    $session = Join-Path ([Environment]::GetFolderPath('MyDocuments')) (
        'My Games\Homeworld Modern\Diagnostics\Mission10-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    Write-Host 'Load your Mission 10 save with your current graphics settings.'
    Write-Host 'Tell Codex when the slow view is ready. Recording will be armed separately.'
    Write-Host 'Windows may ask approval for its performance recorder before the game opens.'
    & $launcher -SessionDirectory $session
} finally {
    foreach ($name in $captureVariables) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
}
