# U15 world generation on the client (integrated server): selftests, then locate a wild
# waystone cell far away, teleport next to it so its chunk generates, wait for
# "wild waystone placed at ..." and screenshot the generated waystone.
# Screenshots: out/worldgen/{selftest,wild_waystone,locate_placed}.png
$r = Invoke-SelfTest 'worldgen.*'
if ($r.Pass -lt 5) { throw "expected at least 5 worldgen selftests to pass, got $($r.Pass)" }
Save-Screenshot 'selftest' | Out-Null

# Stand-in block when the waystone blocks unit is absent (ignored when bedstones:<style> exists).
$mark = Get-LogMark
Invoke-Chat '/waystones worldgen testblock stone_bricks'
Wait-LogLine 'worldgen test block: minecraft:stone_bricks' 15 -Since $mark | Out-Null

$x = Get-Random -Minimum 20000 -Maximum 200000
$z = Get-Random -Minimum 20000 -Maximum 200000
$mark = Get-LogMark
Invoke-Chat "/waystones worldgen locate $x $z"
$line = Wait-LogLine 'worldgen locate: nearest wild waystone at block (-?\d+) (-?\d+) .*: pending' 15 -Since $mark
if ($line -notmatch 'at block (-?\d+) (-?\d+)') { throw "unexpected locate line: $line" }
$bx = [int]$Matches[1]; $bz = [int]$Matches[2]
Write-Step "wild waystone cell at $bx ~ $bz"

# Stand 4 blocks south-east of it (same height: flat world) and look at it.
$mark = Get-LogMark
Invoke-Chat "/tp @s $($bx + 4) ~ $($bz + 4) facing $bx ~1 $bz"
$placed = Wait-LogLine "wild waystone (placed|skipped) at $bx -?\d+ $bz" 90 -Since $mark
Write-Step $placed
if ($placed -notmatch 'placed at (-?\d+) (-?\d+) (-?\d+)') { throw "the wild waystone was not placed: $placed" }
$by = [int]$Matches[2]
# Look at the waystone's upper half from the exact spot (the first /tp ran before the height was known).
Invoke-Chat "/tp @s $($bx + 3) $by $($bz + 3) facing $bx $($by + 1) $bz"
Start-Sleep -Seconds 4
Save-Screenshot 'wild_waystone' | Out-Null

$mark = Get-LogMark
Invoke-Chat "/waystones worldgen locate $x $z"
Wait-LogLine "worldgen locate: nearest wild waystone at block $bx $bz .*: placed y=$by" 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'locate_placed' | Out-Null
Invoke-Chat '/waystones worldgen testblock off'
