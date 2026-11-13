[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$GameRoot,[switch]$CheckOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$gameRootPath=(Resolve-Path -LiteralPath $GameRoot).Path
$gameExe=Join-Path $gameRootPath 'OpenHW-Daignostics.exe'
$version=Get-Content -LiteralPath (Join-Path $gameRootPath 'version.json') -Raw | ConvertFrom-Json
$actual=(Get-FileHash -LiteralPath $gameExe -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actual -ne $version.diagnostic) { throw 'The diagnostics EXE differs from the installed release. Reapply the current patch before capturing.' }
if ($CheckOnly) { Write-Output "Engine-bulb capture ready: OpenHW $($version.version), current diagnostic EXE verified. No game launched."; return }
if (Get-Process -Name OpenHW,OpenHW-Daignostics,OpenHW-Diagnostics,HomeworldModern -ErrorAction SilentlyContinue) {
    throw 'Close the current game before starting this diagnostic run.'
}
$session=Join-Path $gameRootPath ('Development\Diagnostics\EngineBulbFG-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$drive=[IO.DriveInfo]::new([IO.Path]::GetPathRoot($session))
if ($drive.AvailableFreeSpace -lt 8GB) { throw 'The capture drive needs 8 GB free for image bursts.' }
[void][IO.Directory]::CreateDirectory((Join-Path $session 'bursts'))
$configuration=Join-Path $gameRootPath 'Homeworld.cfg'
if (Test-Path -LiteralPath $configuration) { Copy-Item -LiteralPath $configuration -Destination (Join-Path $session 'settings-start.cfg') }
$env:HW_PORTABLE_ROOT=$gameRootPath
$env:HW_TEMPORAL_CAPTURE='1'
$env:HW_FG_ZOOM_CAPTURE='0'
$env:HW_FG_ZOOM_CAPTURE_DIR=Join-Path $session 'bursts'
$env:HW_FG_ZOOM_CAPTURE_ARM_FILE=Join-Path $session 'request.txt'
$env:HW_FSR_FG_DEBUG='1'
Write-Host 'Load the affected scene with frame generation ON and the engine bulb visible.'
Write-Host 'Tell Codex when ready. Image capture will be armed separately.'
Write-Host 'Image readback can briefly pause the game; this run is for visual diagnosis.'
$process=Start-Process -FilePath $gameExe -WorkingDirectory $gameRootPath -WindowStyle Normal -PassThru `
    -RedirectStandardError (Join-Path $session 'renderer.log') -RedirectStandardOutput (Join-Path $session 'stdout.log')
$metadata=[ordered]@{
    started=(Get-Date).ToString('o'); gameId=$process.Id; version=$version.version
    sha256=$actual; sessionDirectory=$session; armFile=$env:HW_FG_ZOOM_CAPTURE_ARM_FILE
    purpose='Engine glow/bulb frame-generation artifact; player controls the camera; settings unchanged'
}
$json=$metadata | ConvertTo-Json -Depth 5
[IO.File]::WriteAllText((Join-Path $session 'session.json'),$json,[Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText((Join-Path $gameRootPath 'Development\Diagnostics\EngineBulbFG-active-session.json'),$json,[Text.UTF8Encoding]::new($false))
$process.WaitForExit()
if (Test-Path -LiteralPath $configuration) { Copy-Item -LiteralPath $configuration -Destination (Join-Path $session 'settings-end.cfg') }
[IO.File]::WriteAllText((Join-Path $session 'finished.json'),(@{closed=(Get-Date).ToString('o');exitCode=$process.ExitCode} | ConvertTo-Json),[Text.UTF8Encoding]::new($false))
Write-Host "Capture session saved: $session"
