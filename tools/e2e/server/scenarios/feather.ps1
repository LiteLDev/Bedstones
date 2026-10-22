# Twinbound Feather (U6): linking ritual, partner targets, warping to the partner, unlisted
# partners and the link helper, plus the item being registered (/give).
$r = Invoke-SelfTest 'feather.*'
if ($r.Pass -lt 7) { throw "expected 7 feather selftests to pass, got $($r.Pass) (fail $($r.Fail))" }
if (-not (Get-ServerLog | Where-Object { $_ -match 'feather linked: .+ <-> .+' })) { throw 'no "feather linked" log line' }
if (-not (Get-ServerLog | Where-Object { $_ -match 'feather warp: .+ -> player:' })) { throw 'no "feather warp" log line' }

$mark = Get-LogMark
Send-Console 'give @a bedstones:twinbound_feather 1'
Wait-LogLine 'No targets matched selector|Gave|twinbound_feather' 10 -Since $mark | Out-Null
if (Get-ServerLog -Since $mark | Where-Object { $_ -match 'not a valid item|Unknown item|Syntax error' }) { throw 'bedstones:twinbound_feather is not a registered item' }
