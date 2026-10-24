# Showcase: a real landscape instead of the flat test world. The BS-Test fixture is switched to
# the infinite generator with a fixed seed (so the spot the scenario uses is the same every run),
# coordinates hidden, peaceful, no daylight cycle. The world is deleted after the run as usual.
. "$PSScriptRoot/../../common/nbt.ps1"
$levelDat = Join-Path (Join-Path (Get-WorldsDir) $script:WorldName) 'level.dat'
$ld = Read-LevelDat $levelDat
Set-NbtValue $ld.Root 'Generator' 3 1
Set-NbtValue $ld.Root 'RandomSeed' 4 ([long]$(if ($env:BS_SHOWCASE_SEED) { $env:BS_SHOWCASE_SEED } else { 20260521 }))
Set-NbtValue $ld.Root 'showcoordinates' 1 ([byte]0)
Write-LevelDat $ld $levelDat
Write-Step "showcase: BS-Test switched to the infinite generator"

