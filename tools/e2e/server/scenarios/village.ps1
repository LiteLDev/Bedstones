# Village waystones in a real generated village (default terrain): locate the nearest village,
# load it with a ticking area, wait for the waystone the worldgen unit places next to its bell and
# check the block in the world.
$mark = Get-LogMark
Send-Console 'locate structure village'
$hit = Wait-LogLine 'is at block (-?\d+), (\S+), (-?\d+)' 30 -Since $mark
if ($hit -notmatch 'is at block (-?\d+), (\S+), (-?\d+)') { throw "unexpected locate output: $hit" }
$vx = [int]$Matches[1]; $vz = [int]$Matches[3]
Write-Step "nearest village around $vx $vz"

$mark = Get-LogMark
Send-Console "tickingarea add circle $vx 64 $vz 4 bs_village true"
$placed = Wait-LogLine 'village waystone placed at (-?\d+) (-?\d+) (-?\d+) in minecraft:overworld \(style (\w+), village' 240 -Since $mark
if ($placed -notmatch 'placed at (-?\d+) (-?\d+) (-?\d+) in minecraft:overworld \(style (\w+)') { throw "unparsable: $placed" }
$x = [int]$Matches[1]; $y = [int]$Matches[2]; $z = [int]$Matches[3]; $style = $Matches[4]
Write-Step "village waystone: $style at $x $y $z"
if ([Math]::Abs($x - $vx) -gt 96 -or [Math]::Abs($z - $vz) -gt 96) { throw "the waystone is not near the located village ($x $z vs $vx $vz)" }

$mark = Get-LogMark
Send-Console "testforblock $x $y $z bedstones:$style"
Wait-LogLine 'Successfully found the block' 15 -Since $mark | Out-Null
$mark = Get-LogMark
Send-Console "testforblock $x $($y + 1) $z bedstones:$style"
Wait-LogLine 'Successfully found the block' 15 -Since $mark | Out-Null
Send-Console 'tickingarea remove bs_village'
