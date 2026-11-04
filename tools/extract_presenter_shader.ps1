param([Parameter(Mandatory)][string]$Source, [Parameter(Mandatory)][string]$Output)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\presenter_shader_source.ps1"
[IO.File]::WriteAllText($Output, (Get-PresenterShaderSource -Source $Source))
