# Smoke: the mod is enabled (run.ps1 waited for "Bedstones enabled"), the player is in the
# BS-Test world, the foundation selftests pass and the native screen pipeline works end to end:
# /waystones debug screen -> client draws it -> click "Done" -> the server gets the action.
# Screenshots: out/smoke/{hud,selftest,debug_screen}.png
Save-Screenshot 'hud' | Out-Null
$r = Invoke-SelfTest 'foundation.*'
if ($r.Pass -lt 10) { throw "expected at least 10 foundation selftests to pass, got $($r.Pass)" }
Save-Screenshot 'selftest' | Out-Null

Wait-LogLine 'companion: server ready' 30 | Out-Null
$mark = Get-LogMark
Invoke-Chat '/waystones debug screen'
Wait-LogLine 'companion: screen foundation_test shown' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'debug_screen' | Out-Null
Click 0.5 0.5 -Relative
Wait-LogLine 'debug screen: .* sent action done' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'closed' | Out-Null
