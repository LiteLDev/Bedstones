# A longer warp plate use time (100 ticks = 5 s) so the scenario can screenshot the warping state.
$cfgDir = Join-Path $script:ModsDir 'Bedstones/config'
New-Item -ItemType Directory -Force $cfgDir | Out-Null
'{ "version": 1, "warpPlateUseTime": 100, "warpPlateCooldownTime": 20 }' | Set-Content (Join-Path $cfgDir 'warpPlate.json') -Encoding utf8NoBOM
