# U8 requirements: the configured rule list is loaded at enable, then the selftests check the
# default costs, cross-dimension cost, free sources, cooldowns (+ sync), creative bypass, a custom
# rule list set at runtime and the XP deduction on a simulated player.
Wait-LogLine 'requirements: 9 warp requirement rule\(s\) active' 30 | Out-Null
$r = Invoke-SelfTest 'requirements.*'
if ($r.Pass -lt 7) { throw "expected 7 requirements selftests to pass, got $($r.Pass)" }

# Reloading the config keeps the default list (no invalid rule warnings).
$mark = Get-LogMark
Send-Console 'waystones reload'
Wait-LogLine 'configuration reloaded' 30 -Since $mark | Out-Null
$r = Invoke-SelfTest 'requirements.default_costs'
if ($r.Pass -lt 1) { throw 'default costs failed after a reload' }
if (Get-ServerLog -Since $mark | Where-Object { $_ -match 'ignoring invalid warp requirement' }) {
    throw 'the default rule list produced parse warnings'
}
