# U9: permissions, visibility and naming.
# - the access module rebuilt its global / team indexes when the world became ready;
# - every canEdit / canManageGlobal / visibilityOptions / canSee rule (selftests access.*);
# - the name generator: presets, templates, Roman-numeral dedup, persistence (selftests naming.*).
Wait-LogLine 'access: indexes rebuilt' 60 -Since 0 | Out-Null

$r = Invoke-SelfTest 'access.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 9) { throw "expected 9 access selftests to pass, got pass=$($r.Pass) fail=$($r.Fail)" }

$mark = Get-LogMark
$r = Invoke-SelfTest 'naming.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 4) { throw "expected 4 naming selftests to pass, got pass=$($r.Pass) fail=$($r.Fail)" }
Wait-LogLine 'name generated: BsU9Test\d+ II \(PRESET_ONLY\)' 10 -Since $mark | Out-Null
Wait-LogLine 'name generated: Cherry Grove .+ \(RANDOM_ONLY\)' 10 -Since $mark | Out-Null
