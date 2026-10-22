# U5: Portal Scroll + Warp Portal, Epitaph + Fleeting Memorial (server side).
# Selftests: the interceptor opens a portal and uses up the scroll, no-space error, entering warps
# and sets returnPortal, expiry, closed portal when the target is gone, charging, epitaph death,
# memorial listing (owner only) and removal after the warp.
$r = Invoke-SelfTest 'portal.*'
if ($r.Pass -lt 6) { throw "expected 6 portal selftests to pass, got $($r.Pass)" }
$r = Invoke-SelfTest 'memorial.*'
if ($r.Pass -lt 4) { throw "expected 4 memorial selftests to pass, got $($r.Pass)" }
# The log lines the client scenario waits for were written by the selftests.
Wait-LogLine 'warp portal opened: .* -> .* by ' 5 | Out-Null
Wait-LogLine 'warp portal used: ' 5 | Out-Null
Wait-LogLine 'warp portal closed: .* \(expired\)' 5 | Out-Null
Wait-LogLine 'fleeting memorial placed: ' 5 | Out-Null
Wait-LogLine 'fleeting memorial used: ' 5 | Out-Null
