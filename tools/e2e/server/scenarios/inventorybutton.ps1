# Inventory button (U14): state per mode, nearest choice (return portal first), the
# `inventory_button.warp` handler and the cooldown, plus the runtime mode command.
$r = Invoke-SelfTest 'inventorybutton.*'
if ($r.Pass -lt 4) { throw "expected 4 inventorybutton selftests to pass, got $($r.Pass)" }

$mark = Get-LogMark
Send-Console 'waystones inventorybutton mode NEAREST'
Wait-LogLine "inventory button mode set to 'NEAREST'" 10 -Since $mark | Out-Null
Send-Console 'waystones inventorybutton mode NONE'
Wait-LogLine "inventory button mode set to 'NONE'" 10 -Since $mark | Out-Null
