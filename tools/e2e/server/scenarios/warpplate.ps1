# U3 warp plate: attunement + ejection, shard insert/eject, B -> A teleport of a simulated player,
# the arrival cooldown, the locked deepslate shard, crumbling shard consumption, use-time maths,
# redstone disabling and silk-touch breaking (selftests in src/feature/warpplate/WarpPlateTests.cpp).
$r = Invoke-SelfTest 'warpplate.*'
if ($r.Pass -lt 11) { throw "expected 11 warpplate selftests to pass, got $($r.Pass) (fail $($r.Fail))" }

# The behavior pack definitions (block, items, recipes) load without content errors.
$packErrors = Get-ServerLog | Where-Object { $_ -match 'ERR' -and $_ -match 'warp_plate|warp_dust|_shard' }
if ($packErrors) { throw "pack errors: $($packErrors -join ' | ')" }

# A plate set by a command gets a record next tick; removing the block removes it.
$mark = Get-LogMark
Send-Console 'tickingarea add 0 -64 0 15 -40 15 bs_warpplate'
Wait-Seconds 2
Send-Console 'setblock 3 -50 3 bedstones:warp_plate'
Wait-LogLine 'warp plate placed: .* by \?' 15 -Since $mark | Out-Null
Send-Console 'setblock 3 -50 3 air'
Wait-LogLine 'warp plate broken: ' 15 -Since $mark | Out-Null
Send-Console 'tickingarea remove bs_warpplate'
