param([Parameter(Mandatory=$true)][string]$Stage,
      [Parameter(Mandatory=$true)][string]$Executable,
      [string]$Install='F:\OpenHWWIP')
$ErrorActionPreference='Stop'
if(Get-Process OpenHW,HomeworldModern -ErrorAction SilentlyContinue){throw 'Close the game before deployment'}
$stageRoot=(Resolve-Path -LiteralPath $Stage).Path
$installRoot=(Resolve-Path -LiteralPath $Install).Path
$exe=(Resolve-Path -LiteralPath $Executable).Path
$changes=@()
Get-ChildItem -LiteralPath $stageRoot -File -Recurse | ForEach-Object {
    $relative=[IO.Path]::GetRelativePath($stageRoot,$_.FullName)
    if($relative -notmatch '^(Data|mods|Dev)[\\/]'){throw 'Unexpected staged path'}
    $destination=Join-Path $installRoot $relative
    $hash=(Get-FileHash -LiteralPath $_.FullName).Hash
    if(!(Test-Path -LiteralPath $destination) -or (Get-FileHash -LiteralPath $destination).Hash -ne $hash){
        if($_.Extension -in '.owshipdef','.owsubsystem'){
            # Reject stale staged changes if the user edited the ship meanwhile.
            $strip={param($text) [regex]::Replace([regex]::Replace($text,'(?m)^\s*WeaponFire\s+[^\n]*',''),'\s+',' ').Trim()}
            if((& $strip (Get-Content -LiteralPath $_.FullName -Raw)) -ne (& $strip (Get-Content -LiteralPath $destination -Raw))){throw "Ship changed since staging: $relative"}
        }
        $changes+=@{source=$_.FullName;target=$destination;relative=$relative;hash=$hash}
    }
}
$changes+=@{source=$exe;target=(Join-Path $installRoot 'OpenHW.exe');relative='OpenHW.exe';hash=(Get-FileHash -LiteralPath $exe).Hash}
$backup=Join-Path $installRoot ('Dev\Backups\weapon-impacts-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Path $backup | Out-Null
foreach($item in $changes){
    if(Test-Path -LiteralPath $item.target){
        $old=Join-Path $backup $item.relative
        New-Item -ItemType Directory -Path (Split-Path $old) -Force | Out-Null
        Copy-Item -LiteralPath $item.target -Destination $old
    }
}
if(Get-Process OpenHW,HomeworldModern -ErrorAction SilentlyContinue){throw 'Game restarted; deployment stopped before copying'}
foreach($item in $changes){
    New-Item -ItemType Directory -Path (Split-Path $item.target) -Force | Out-Null
    Copy-Item -LiteralPath $item.source -Destination $item.target -Force
    if((Get-FileHash -LiteralPath $item.target).Hash -ne $item.hash){throw "Verification failed: $($item.relative)"}
}
[pscustomobject]@{files=$changes.Count;backup=$backup;exeHash=(Get-FileHash -LiteralPath (Join-Path $installRoot 'OpenHW.exe')).Hash}
