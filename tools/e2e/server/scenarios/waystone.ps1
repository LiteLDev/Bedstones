# U1 waystone blocks: the selftests (placement, two-tall integrity, records, activation and naming,
# breaking with / without silk touch and restoring, shift-use -> Edit, the real interact path),
# then the block states from the behavior pack through commands.
$r = Invoke-SelfTest 'waystone.*'
if ($r.Pass -lt 12) { throw "expected at least 12 waystone selftests to pass, got $($r.Pass)" }

# The blocks are registered with their integer states (a command sets both halves).
Send-Console 'tickingarea add 16 -64 16 24 0 24 bs_u1 true'
Wait-Seconds 3
$mark = Get-LogMark
Send-Console 'setblock 20 -60 20 bedstones:end_stone_waystone ["bedstones:facing"=2,"bedstones:upper"=0]'
Send-Console 'setblock 20 -59 20 bedstones:end_stone_waystone ["bedstones:facing"=2,"bedstones:upper"=1]'
Wait-LogLine 'Block placed' 15 -Since $mark | Out-Null
# Removing one half by command removes the other one as well (next tick).
$mark = Get-LogMark
Send-Console 'setblock 20 -59 20 air'
Wait-Seconds 1
Send-Console 'testforblock 20 -60 20 air'
Wait-LogLine 'Successfully found the block' 15 -Since $mark | Out-Null
