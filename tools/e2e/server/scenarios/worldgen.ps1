# U15 world generation: selftests (grid, style by biome, placement + records, dimension filters,
# persistence), then the real chunk-load path: locate a never-generated wild waystone cell far
# away, load its chunk with a ticking area and wait for the generated waystone.
$r = Invoke-SelfTest 'worldgen.*'
if ($r.Pass -lt 5) { throw "expected at least 5 worldgen selftests to pass, got $($r.Pass)" }

# Without the blocks unit the style blocks are missing: generate a stand-in block instead
# (ignored when bedstones:<style> exists).
$mark = Get-LogMark
Send-Console 'waystones worldgen testblock stone_bricks'
Wait-LogLine 'worldgen test block: minecraft:stone_bricks' 15 -Since $mark | Out-Null

# A random far column: its cell has never been generated in this world.
$x = Get-Random -Minimum 100000 -Maximum 900000
$z = Get-Random -Minimum 100000 -Maximum 900000
$mark = Get-LogMark
Send-Console "waystones worldgen locate $x $z"
$line = Wait-LogLine 'worldgen locate: nearest wild waystone at block (-?\d+) (-?\d+) .*: pending' 15 -Since $mark
if ($line -notmatch 'at block (-?\d+) (-?\d+)') { throw "unexpected locate line: $line" }
$bx = [int]$Matches[1]; $bz = [int]$Matches[2]
Write-Step "wild waystone cell at $bx ~ $bz"

$mark = Get-LogMark
Send-Console "tickingarea add circle $bx 0 $bz 3 bs_worldgen true"
$placed = Wait-LogLine "wild waystone (placed|skipped) at $bx -?\d+ $bz" 120 -Since $mark
Write-Step $placed
if ($placed -notmatch 'placed') { throw "the wild waystone was not placed: $placed" }

$mark = Get-LogMark
Send-Console "waystones worldgen locate $x $z"
Wait-LogLine "worldgen locate: nearest wild waystone at block $bx $bz .*: placed y=-?\d+" 15 -Since $mark | Out-Null

Send-Console 'tickingarea remove bs_worldgen'
Send-Console 'waystones worldgen testblock off'
Wait-Seconds 1
