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
$referenceSave = Join-Path $gameRoot 'SavedGames\SinglePlayer\14'
if (!(Test-Path -LiteralPath $referenceSave -PathType Leaf)) { throw 'Mission 14 save named 14 is missing.' }
if ($CheckOnly) {
    Write-Output 'Mission 14 diagnostics verified. No game or recorder started; no settings changed.'
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
        'My Games\Homeworld Modern\Diagnostics\Mission14-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    if (Get-Process -Name OpenHW,OpenHW-Diagnostics,OpenHW-PerformanceTest,OpenHW-FGZoomCapture,HomeworldModern -ErrorAction SilentlyContinue) {
        throw 'Close the existing game before starting Mission 14 diagnostics.'
    }
    if (Get-CimInstance Win32_Process -Filter "Name='OpenHW.exe' OR Name='OpenHW-Diagnostics.exe' OR Name='OpenHW-PerformanceTest.exe' OR Name='OpenHW-FGZoomCapture.exe' OR Name='HomeworldModern.exe'") {
        throw 'Close the existing game before starting Mission 14 diagnostics.'
    }
    $referenceDir = Join-Path $PSScriptRoot ('Mission14Performance\Reference-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    [void][IO.Directory]::CreateDirectory($referenceDir)
    Copy-Item -LiteralPath $referenceSave -Destination (Join-Path $referenceDir '14')
    $saveHash = (Get-FileHash -LiteralPath $referenceSave).Hash
    if ((Get-FileHash -LiteralPath (Join-Path $referenceDir '14')).Hash -ne $saveHash) { throw 'Reference save backup verification failed.' }
    Copy-Item -LiteralPath (Join-Path $gameRoot 'Homeworld.cfg') -Destination (Join-Path $referenceDir 'settings.cfg')
    $authoredHashes = [ordered]@{}
    foreach ($directory in @('RTXLighting','Data\Missions','RTXExports\Maps','Dev\RTXExports\Maps')) {
        $path = Join-Path $gameRoot $directory
        if (Test-Path -LiteralPath $path) {
            foreach ($file in Get-ChildItem -LiteralPath $path -Recurse -File | Where-Object Extension -In @('.rtxmap','.rtxlight','.rtxdust')) {
                $authoredHashes[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName).Hash
            }
        }
    }
    $plan = [ordered]@{
        mission = 14; save = $referenceSave; saveSha256 = $saveHash
        referenceDirectory = $referenceDir; sessionDirectory = $session
        executableSha256 = $manifest.sha256; symbolsSha256 = $manifest.symbolsSha256
        settingsSha256 = (Get-FileHash -LiteralPath (Join-Path $gameRoot 'Homeworld.cfg')).Hash
        authoredFileHashes = $authoredHashes
        continuous = @('Engine diagnostic log','CPU and RAM process samples','GPU utilization, VRAM, clocks, power and temperature')
        requestedCapture = @('45-second ETW CPU stacks, GPU queues/residency, disk IO and hard faults','PresentMon presentation timings','Engine five-second warmup and 30-second base-frame CPU/GPU pass timing, cache and hitch report')
        procedure = 'Load save 14; report ready at a slow view. Codex starts the bounded trace using a marker. Keep the same visual settings; use normal camera movements. Camera is controlled by the player.'
        purpose = 'Find the measured bottlenecks while preserving authored visuals. No visual readbacks or counterfactual rendering during performance capture.'
        started = (Get-Date).ToString('o')
    }
    $json = $plan | ConvertTo-Json -Depth 8
    [IO.File]::WriteAllText((Join-Path $referenceDir 'plan.json'),$json,[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $PSScriptRoot 'Mission14-performance-plan.json'),$json,[Text.UTF8Encoding]::new($false))
    Write-Host 'Load your Mission 14 save named 14 with your current graphics settings.' 
    Write-Host 'Tell Codex when the slow view is ready. Recording will be armed separately.'
    Write-Host 'Windows may ask approval for its performance recorder before the game opens.'
    & $launcher -SessionDirectory $session
} finally {
    foreach ($name in $captureVariables) {
        [Environment]::SetEnvironmentVariable($name, $savedEnvironment[$name], 'Process')
    }
}
