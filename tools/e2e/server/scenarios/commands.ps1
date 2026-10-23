# U16 admin commands: the commands.* selftests (activate / forget / place / count / list /
# cooldown / twinbound / gui handlers with their exact output) plus the real command parser from
# the server console (`waystones place` with an unknown and a known style).
$r = Invoke-SelfTest 'commands.*'
if ($r.Pass -lt 8) { throw "expected at least 8 commands selftests to pass, got $($r.Pass)" }

$mark = Get-LogMark
Send-Console 'waystones place 0 200 0 granite_waystone Nope'
Wait-LogLine 'Unknown waystone style: granite_waystone' 20 -Since $mark | Out-Null

$mark = Get-LogMark
Send-Console 'waystones place 0 200 0 mossy_waystone Console Stone'
Wait-LogLine 'commands: placed waystone Console Stone \(mossy_waystone\)|Could not place bedstones:mossy_waystone' 20 -Since $mark | Out-Null
