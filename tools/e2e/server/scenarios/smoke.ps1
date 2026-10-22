# Smoke: the mod is enabled (run.ps1 already waited for "Bedstones enabled") and the
# foundation selftests pass (registry, player data, screens protocol, default seams).
$r = Invoke-SelfTest 'foundation.*'
if ($r.Pass -lt 7) { throw "expected at least 7 foundation selftests to pass, got $($r.Pass)" }
