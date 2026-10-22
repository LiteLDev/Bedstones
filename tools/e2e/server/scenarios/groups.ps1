# U12 Groups: IGroups (inbuilt + user groups, hidden state, order, groupsOf) and the Manage Groups /
# Edit Group screen actions, driven through the screens protocol by selftests (simulated player
# with a companion), plus the foundation's default-seam test against the real implementation.
$r = Invoke-SelfTest 'groups.*'
if ($r.Pass -lt 7) { throw "expected at least 7 groups selftests to pass, got $($r.Pass)" }
$f = Invoke-SelfTest 'foundation.groups'
if ($f.Pass -lt 1) { throw 'foundation.groups failed with the U12 IGroups implementation' }
# The debug subcommand needs a player: from the console it must refuse cleanly.
$mark = Get-LogMark
Send-Console 'waystones debug groups'
Wait-LogLine 'Only players can open screens' 15 -Since $mark | Out-Null
