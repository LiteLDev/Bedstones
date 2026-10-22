# U17 visuals: the `visuals.waystones` sync for a test companion (payload contents, activation
# flags, range filtering, re-sends on activate / forget / record changes / moving).
$r = Invoke-SelfTest 'visuals.*'
if ($r.Pass -lt 4) { throw "expected 4 visuals selftests to pass, got $($r.Pass)" }

# The debug command exists and refuses the console (no player).
$mark = Get-LogMark
Send-Console 'waystones visuals clear'
Wait-LogLine 'Only players can use this' 10 -Since $mark | Out-Null
