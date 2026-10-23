# U11 Manage Waystones + Personal Settings: every server rule through simulated screen actions
# (reorder, delete rules, hide/show, alias, favorite, group cycle, validation, edit link).
$r = Invoke-SelfTest 'manage.*'
if ($r.Pass -lt 12) { throw "expected 12 manage selftests to pass, got $($r.Pass)" }
Wait-LogLine "manage: swapped 'M Swap A' <-> 'M Swap B' by" 5 | Out-Null
Wait-LogLine "manage: deleted 'M Share Target' \(purge\)" 5 | Out-Null
Wait-LogLine "manage: deleted 'M Global Gone' \(demote\)" 5 | Out-Null
Wait-LogLine "personal settings: saved 'M Alias' by .* \(alias 'My Spot'" 5 | Out-Null
