# U2: sharestones and portstones. The selftests place the blocks with a simulated player's item use,
# right-click them (empty hand / sneaking), check the records, the same-colour list of ITargets,
# the portstone colour rule and that breaking either half removes both (and the record).
$r = Invoke-SelfTest 'sharestone.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 5) { throw "sharestone selftests: pass=$($r.Pass) fail=$($r.Fail)" }
Wait-LogLine 'sharestone placed: red_sharestone at .* by bs_test_' 5 | Out-Null
Wait-LogLine 'sharestone removed: ' 5 | Out-Null
$r = Invoke-SelfTest 'portstone.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 2) { throw "portstone selftests: pass=$($r.Pass) fail=$($r.Fail)" }
Wait-LogLine 'portstone used: white by bs_test_' 5 | Out-Null
