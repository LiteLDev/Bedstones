# Showcase: stages and captures the screenshots and videos of docs/showcase/ in a real world
# (seed 20260521, infinite generator, see showcase.prelaunch.ps1): a grassy meadow by the sea,
# a stream glade to the north, a sandy beach south of the spawn.
#   waystone      the "Seaside Meadow" waystone, activated (runes lit)
#   selection     the Waystones menu opened by right-clicking it
#   teleport      video: right-click -> menu -> pick "Hilltop Grove" -> arrive there
#   edit          the Edit Waystone screen (shift + right-click)
#   styles        the 10 waystone styles in a row on the beach
#   sharestones   four sharestones placed by hand
#   warpplate     two warp plates, one holding the other's attuned shard
# Output: out/showcase/*.png and out/showcase/frames-*/ (JPEG frames + ffmpeg concat list with the
# real frame durations). Encoding to WebP/MP4 is done by hand with ffmpeg (see CONTRIBUTING.md).
. "$PSScriptRoot/showcase.capture.ps1"

Wait-LogLine 'companion: server ready' 60 | Out-Null
foreach ($c in '/gamerule sendcommandfeedback false', '/gamerule commandblockoutput false', '/gamerule dodaylightcycle false',
    '/time set 5000', '/weather clear', '/gamemode creative', '/clear @s', '/effect @s clear') { Invoke-Chat $c }

function Invoke-Step([scriptblock]$Do, [string]$Pattern, [int]$Attempts = 3, [int]$Seconds = 6) {
    for ($i = 1; $i -le $Attempts; $i++) {
        $m = Get-LogMark
        & $Do
        try { return Wait-LogLine $Pattern $Seconds -Since $m } catch { Write-Step "step not seen (attempt $i): $Pattern" }
    }
    throw "step failed after $Attempts attempts: $Pattern"
}

# Places a named waystone where the player stands after landing on the surface at (x, z), and
# returns its block position (read back from the log line of /waystones place).
function Place-Waystone([int]$x, [int]$z, [string]$style, [string]$name) {
    # Load the chunks from above first: /spreadplayers only lands on loaded ground.
    Invoke-Chat "/tp @s $x 140 $z"
    Start-Sleep -Seconds 4
    Invoke-Chat "/spreadplayers $x $z 0 1 @s"
    Start-Sleep -Seconds 2
    $line = Invoke-Step { Invoke-Chat "/waystones place ~ ~ ~ $style $name" } "commands: placed waystone $([regex]::Escape($name)) \(\S+\) at -?\d+ -?\d+ -?\d+ dim"
    $null = $line -match 'at (-?\d+) (-?\d+) (-?\d+) dim'
    $p = [pscustomobject]@{ X = [int]$Matches[1]; Y = [int]$Matches[2]; Z = [int]$Matches[3]; Name = $name }
    if ([Math]::Abs($p.X - $x) -gt 2 -or [Math]::Abs($p.Z - $z) -gt 2) { throw "'$name' landed at $($p.X) $($p.Z), not near $x $z" }
    # (Relative positions: absolute ones did not find the record for /waystones activate.)
    Invoke-Step { Invoke-Chat '/waystones activate @s ~ ~ ~' } "commands: activated waystone $([regex]::Escape($name)) for" | Out-Null
    Invoke-Chat "/tp @s $($p.X + 0.5) $($p.Y) $($p.Z - 2.5)"
    Write-Step "waystone '$name' at $($p.X) $($p.Y) $($p.Z)"
    return $p
}

# A flat patch: ground block $ground at y-1, air from y to y+4.
function Flatten([int]$x1, [int]$z1, [int]$x2, [int]$z2, [int]$y, [string]$ground) {
    Invoke-Chat "/fill $x1 $($y - 2) $z1 $x2 $($y - 2) $z2 dirt"
    Invoke-Chat "/fill $x1 $($y - 1) $z1 $x2 $($y - 1) $z2 $ground"
    Invoke-Chat "/fill $x1 $y $z1 $x2 $($y + 4) $z2 air"
}

# A still through the free camera, HUD hidden.
function Shot([string]$name, [string]$pos, [string]$facing, [int]$settle = 3) {
    Invoke-Chat '/tp @s ~ ~ ~ 0 -90'   # look at the sky: no block outline in the shot
    if (-not $script:Invisible) { Invoke-Chat '/effect @s invisibility 99999 0 true'; $script:Invisible = $true }   # keep the player model out of the shot
    Invoke-Chat "/camera @s set minecraft:free pos $pos facing $facing"
    Start-Sleep -Seconds $settle
    Save-Screenshot $name | Out-Null
}
function End-Shots { Invoke-Chat '/camera @s clear'; Invoke-Chat '/effect @s clear'; $script:Invisible = $false; Start-Sleep -Milliseconds 500 }
function F([double]$v) { $v.ToString([Globalization.CultureInfo]::InvariantCulture) }

# ---- waystones ------------------------------------------------------------------------------------
$beach = Place-Waystone 2 -2 'sandy_waystone' 'Spawn Beach'
$glade = Place-Waystone -50 -60 'mossy_waystone' 'Hilltop Grove'
$point = Place-Waystone -25 15 'prismarine_waystone' 'Sandy Point'
$hill  = Place-Waystone -66 -70 'deepslate_waystone' 'Old Oak Hill'
$meadow  = Place-Waystone -44 -30 'waystone' 'Seaside Meadow'
# A few flowers in front of the meadow waystone.
foreach ($f in @(@(-2, -2, 'poppy'), @(2, -3, 'dandelion'), @(-3, 1, 'oxeye_daisy'), @(3, 0, 'cornflower'), @(-1, -4, 'azure_bluet'))) {
    $fx = $meadow.X + $f[0]; $fz = $meadow.Z + $f[1]
    Invoke-Chat "/execute positioned $fx $($meadow.Y) $fz if block ~ ~-1 ~ grass_block run setblock ~ ~ ~ $($f[2])"
}

Invoke-Chat '/hud @s hide all'
$hx = $meadow.X + 0.5; $hy = $meadow.Y; $hz = $meadow.Z + 0.5
Shot 'waystone' "$(F ($hx - 2.6)) $(F ($hy + 1.5)) $(F ($hz - 4.6))" "$(F $hx) $(F ($hy + 1.0)) $(F $hz)"
Shot 'waystone_b' "$(F ($hx + 1.8)) $(F ($hy + 1.0)) $(F ($hz - 3.4))" "$(F $hx) $(F ($hy + 1.1)) $(F $hz)"
Shot 'waystone_c' "$(F ($hx - 4.0)) $(F ($hy + 2.6)) $(F ($hz - 7.0))" "$(F $hx) $(F ($hy + 0.8)) $(F $hz)"
$gx = $glade.X + 0.5; $gy = $glade.Y; $gz = $glade.Z + 0.5
Shot 'glade' "$(F ($gx + 2.6)) $(F ($gy + 1.6)) $(F ($gz - 4.6))" "$(F $gx) $(F ($gy + 1.0)) $(F $gz)"
End-Shots
Invoke-Chat '/hud @s reset all'

# ---- menu + teleport video (survival, with XP for the costs) ----------------------------------------
Invoke-Chat '/gamemode survival'
Invoke-Chat '/xp 30L @s'
Invoke-Chat "/tp @s $(F $hx) $hy $(F ($hz - 2.6)) facing $(F $hx) $(F ($hy - 0.6)) $(F $hz)"
Start-Sleep -Seconds 11      # let the chat fade
Start-FrameCapture 'teleport'
Start-Sleep -Milliseconds 1200
$m = Get-LogMark
Invoke-Step { Click 0.5 0.5 -Relative -Right } 'waystone selection opened: .+ by' | Out-Null
Wait-LogLine 'companion: screen selection shown' 15 -Since $m | Out-Null
$line = Wait-LogLine 'selection screen: layout ' 10 -Since $m
$null = $line -match 'screen=([\d.]+)x([\d.]+) .* list=([-\d.]+),([-\d.]+)'
$W = [double]$Matches[1]; $H = [double]$Matches[2]; $LX = [double]$Matches[3]; $LY = [double]$Matches[4]
$rowsLine = @(Get-LeviLog -Since $m | Where-Object { $_ -match 'selection screen: rows=' }) | Select-Object -Last 1
$null = $rowsLine -match 'rows=(.*) sort='
$rows = @($Matches[1] -split '\|')
Write-Step "rows: $($rows -join ', ')"
$i = [array]::IndexOf($rows, 'Hilltop Grove')
$rx = ($LX + 110) / $W; $ry = ($LY + 22 * $i + 11) / $H
Start-Sleep -Milliseconds 900
Move-Mouse (($LX + 150) / $W) (($LY + 22 * ($rows.Count) + 30) / $H) -Relative
Start-Sleep -Milliseconds 500
Move-Mouse $rx $ry -Relative
Start-Sleep -Milliseconds 900
$m = Get-LogMark
Click $rx $ry -Relative
Wait-LogLine 'warp: .* -> Hilltop Grove' 20 -Since $m | Out-Null
Start-Sleep -Milliseconds 3500
Stop-FrameCapture
Save-Screenshot 'arrived' | Out-Null

# The menu as a still (back at the meadow, hovering the destination).
Invoke-Chat "/tp @s $(F $hx) $hy $(F ($hz - 2.6)) facing $(F $hx) $(F ($hy - 0.6)) $(F $hz)"
Start-Sleep -Seconds 11
$m = Get-LogMark
Invoke-Step { Click 0.5 0.5 -Relative -Right } 'waystone selection opened: .+ by' | Out-Null
Wait-LogLine 'companion: screen selection shown' 15 -Since $m | Out-Null
Start-Sleep -Milliseconds 800
Move-Mouse $rx $ry -Relative
Start-Sleep -Milliseconds 600
Save-Screenshot 'selection' | Out-Null
Close-Screen

# Edit Waystone: shift + right-click.
try {
    $m = Get-LogMark
    [BsE2E.Native]::Key(0x10, $false); Start-Sleep -Milliseconds 400
    Click 0.5 0.5 -Relative -Right
    Start-Sleep -Milliseconds 300
    [BsE2E.Native]::Key(0x10, $true)
    Wait-LogLine 'companion: screen edit shown' 10 -Since $m | Out-Null
    Start-Sleep -Milliseconds 800
    Save-Screenshot 'edit' | Out-Null
    Close-Screen
} catch { Write-Step "edit screen not captured: $_"; [BsE2E.Native]::Key(0x10, $true) }
Invoke-Chat '/gamemode creative'

# ---- the 10 styles on the beach --------------------------------------------------------------------
$styles = 'waystone', 'mossy_waystone', 'sandy_waystone', 'deepslate_waystone', 'blackstone_waystone',
    'end_stone_waystone', 'red_nether_bricks_waystone', 'purpur_waystone', 'prismarine_waystone', 'mud_bricks_waystone'
$first = Place-Waystone -30 4 $styles[0] 'Style 1'
$sy = $first.Y; $sx = $first.X; $sz = $first.Z
Flatten ($sx - 3) ($sz - 4) ($sx + 21) ($sz + 2) $sy 'sand'
for ($k = 0; $k -lt $styles.Count; $k++) {
    # ~ resolves by truncation here, so -28.5 is block -28: stand half a block toward zero.
    Invoke-Chat "/tp @s $(if ($sx + 2 * $k -lt 0) { $sx + 2 * $k - 0.5 } else { $sx + 2 * $k + 0.5 }) $sy $(if ($sz -lt 0) { $sz - 0.5 } else { $sz + 0.5 })"
    Start-Sleep -Milliseconds 500
    Invoke-Step { Invoke-Chat "/waystones place ~ ~ ~ $($styles[$k]) Style $($k + 1)" } "commands: placed waystone Style $($k + 1) \(\S+\) at $($sx + 2 * $k) $sy $sz " | Out-Null
    Invoke-Step { Invoke-Chat '/waystones activate @s ~ ~ ~' } "commands: activated waystone Style $($k + 1) for" | Out-Null
}
Invoke-Chat "/tp @s $($sx + 9.5) $sy $($sz - 8)"
Invoke-Chat '/hud @s hide all'
Shot 'styles' "$(F ($sx + 9.5)) $(F ($sy + 2.4)) $(F ($sz - 9.5))" "$(F ($sx + 9.5)) $(F ($sy + 1.0)) $(F ($sz + 0.5))"
Shot 'styles_b' "$(F ($sx - 2.5)) $(F ($sy + 1.8)) $(F ($sz - 5.5))" "$(F ($sx + 7.5)) $(F ($sy + 1.0)) $(F ($sz + 0.5))"
End-Shots
Invoke-Chat '/hud @s reset all'

# ---- sharestones, placed by hand ---------------------------------------------------------------------
$shZ = $sz + 9
Flatten ($sx + 2) ($shZ - 4) ($sx + 16) ($shZ + 2) $sy 'sand'
$colours = 'red', 'blue', 'lime', 'yellow'
for ($k = 0; $k -lt $colours.Count; $k++) {
    $x = $sx + 5 + 3 * $k
    Invoke-Chat "/replaceitem entity @s slot.hotbar 0 bedstones:$($colours[$k])_sharestone"
    Press-Key '1'
    Invoke-Chat "/tp @s $($x + 0.5) $sy $($shZ - 2 + 0.5) facing $($x + 0.5) $($sy - 1.62) $($shZ + 0.5)"
    Start-Sleep -Milliseconds 1200
    Invoke-Step { Click 0.5 0.5 -Relative -Right } "sharestone placed: $($colours[$k])_sharestone at" | Out-Null
    Start-Sleep -Milliseconds 1200
    Close-Screen
}
Invoke-Chat "/tp @s $($sx + 10) $sy $($shZ - 7)"
Invoke-Chat '/hud @s hide all'
Shot 'sharestones' "$(F ($sx + 10)) $(F ($sy + 2.0)) $(F ($shZ - 6.0))" "$(F ($sx + 10)) $(F ($sy + 1.1)) $(F ($shZ + 0.5))"
Shot 'sharestones_b' "$(F ($sx + 3.2)) $(F ($sy + 1.4)) $(F ($shZ - 3.8))" "$(F ($sx + 10)) $(F ($sy + 1.2)) $(F ($shZ + 0.5))"
End-Shots
Invoke-Chat '/hud @s reset all'

# ---- warp plates next to the meadow waystone ---------------------------------------------------------
$py = $meadow.Y; $pz = $meadow.Z - 1; $pa = $meadow.X + 4; $pb = $meadow.X + 7
Flatten ($pa - 1) ($pz - 3) ($pb + 1) ($pz + 1) $py 'grass_block'
Invoke-Chat '/clear @s'
Invoke-Chat '/replaceitem entity @s slot.hotbar 0 bedstones:warp_plate 2'
Press-Key '1'
$plates = @()
foreach ($x in $pa, $pb) {
    Invoke-Chat "/tp @s $($x + 0.5) $py $($pz - 2 + 0.5) facing $($x + 0.5) $($py - 1.62) $($pz + 0.5)"
    Start-Sleep -Milliseconds 1200
    $line = Invoke-Step { Click 0.5 0.5 -Relative -Right } 'warp plate placed: .* by (?!\?)'
    $null = $line -match 'warp plate placed: (.+) \((\w*)\) at'
    $plates += $Matches[1]
    Wait-LogLine "warp plate shard ejected: attuned shard from $([regex]::Escape($Matches[1]))" 15 | Out-Null
}
# Pick up plate A's shard and put it into plate B: B now warps to A.
Invoke-Chat "/tp @s $($pa + 0.5) $($py + 0.2) $($pz + 0.5)"
Start-Sleep -Milliseconds 1500
Press-Key '2'
Invoke-Chat "/tp @s $($pb + 0.5) $py $($pz - 3) facing $($pb + 0.5) $($py - 1.52) $($pz + 0.5)"
Start-Sleep -Milliseconds 1000
Invoke-Step { Click 0.5 0.5 -Relative -Right } "warp plate shard inserted: .* into $([regex]::Escape($plates[1]))" | Out-Null
Invoke-Chat "/tp @s $($pa - 3) $py $($pz - 6)"
Invoke-Chat '/hud @s hide all'
$mx = ($pa + $pb) / 2 + 0.5
Shot 'warpplate' "$(F $mx) $(F ($py + 2.2)) $(F ($pz - 3.2))" "$(F $mx) $(F $py) $(F ($pz + 0.5))"
Shot 'warpplate_b' "$(F ($pa - 1.5)) $(F ($py + 1.6)) $(F ($pz - 2.5))" "$(F ($pb + 0.5)) $(F ($py + 0.2)) $(F ($pz + 0.5))"
End-Shots
Invoke-Chat '/hud @s reset all'
Assert-NoErrors
