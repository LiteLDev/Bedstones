# U7 teleport pipeline: every teleport.* selftest passes on BDS (landing spot + fallback,
# cross-dimension, chunk preloading, leashed mobs, pets, warp item, source range, missing target,
# requirements, interceptors, listeners, menus closing), and the foundation's default-seam tests
# still pass next to the replaced ITeleport.
$mark = Get-LogMark
$r = Invoke-SelfTest 'teleport.*'
if ($r.Pass -lt 13) { throw "expected 13 teleport selftests to pass, got $($r.Pass)" }
Wait-LogLine 'warp: bs_test_\d+ -> Target' 5 -Since $mark | Out-Null
Wait-LogLine 'warp refused: bs_test_\d+ -> .*: chat\.waystones\.source_waystone_out_of_range' 5 -Since $mark | Out-Null
Wait-LogLine 'teleport: closed screen teleport_test of bs_test_\d+' 5 -Since $mark | Out-Null

$f = Invoke-SelfTest 'foundation.*'
if ($f.Pass -lt 8) { throw "expected the 8 foundation selftests to pass, got $($f.Pass)" }
