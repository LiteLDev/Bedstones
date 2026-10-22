# U4 scrolls: selftests for binding, bound / return scroll warps, consumption through the warp
# listener, warp stone durability, creative, the selection seam and the charge loop. The items
# come from the behavior pack (the tests fail if any of them is not registered).
$r = Invoke-SelfTest 'scrolls.*'
if ($r.Pass -lt 7) { throw "expected 7 scrolls selftests to pass, got $($r.Pass) (fail $($r.Fail))" }

# The user-visible actions log one line each (e2e / ops can follow them).
$mark = Get-LogMark
Send-Console 'waystones selftest "scrolls.bind_blank"'
Wait-LogLine 'scroll bound: Bind Target' 30 -Since $mark | Out-Null
$mark = Get-LogMark
Send-Console 'waystones selftest "scrolls.bound_warp"'
Wait-LogLine 'scroll consumed: bedstones:bound_scroll' 30 -Since $mark | Out-Null
