# U10 selection screen: target sets per source, sorting + persistence, select validation, the
# admin variant, the model sent to companions and the form fallback (selftests), plus the debug
# setup command.
$r = Invoke-SelfTest 'selection.*'
if ($r.Pass -lt 12) { throw "expected at least 12 selection selftests to pass, got $($r.Pass)" }

# The foundation's default-seam tests must keep passing with U10's ITargets / ISelection.
$f = Invoke-SelfTest 'foundation.*'
if ($f.Pass -lt 8) { throw "expected the foundation selftests to pass with U10 installed, got $($f.Pass)" }

$mark = Get-LogMark
Send-Console 'waystones debug selection costs'
Wait-LogLine 'selection debug: requirement preview on' 10 -Since $mark | Out-Null
Send-Console 'waystones debug selection costs'
Wait-LogLine 'selection debug: requirement preview off' 10 -Since $mark | Out-Null
