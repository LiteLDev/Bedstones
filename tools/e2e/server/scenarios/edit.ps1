# U13: Edit Waystone screen + Warp Modifiers (save validation, visibility options, modifier count,
# container moves through the screen actions, arrival effects, potency).
$r = Invoke-SelfTest 'edit.*'
if ($r.Pass -lt 5) { throw "expected at least 5 edit selftests to pass, got $($r.Pass)" }
$r = Invoke-SelfTest 'modifiers.*'
if ($r.Pass -lt 5) { throw "expected at least 5 modifiers selftests to pass, got $($r.Pass)" }

# The save log line e2e scenarios wait for.
$mark = Get-LogMark
Send-Console 'waystones selftest "edit.save_rules"'
Wait-LogLine 'edit: waystone saved Untitled Waystone' 60 -Since $mark | Out-Null
