param([ValidateSet('0','1')][string]$AtmosphereCache = '1')
$ErrorActionPreference = 'Stop'
$game = Join-Path $PSScriptRoot 'HomeworldModern.exe'
if (-not (Test-Path -LiteralPath $game -PathType Leaf)) { throw 'Benchmark launcher must be beside the game executable.' }
$env:HW_OPT_ATMOS_CACHE = $AtmosphereCache
# Only invoked by the player's shortcut. Never changes persistent graphics,
# missions or saves. The process environment supplies the single A/B switch.
& $game '-benchmark'
