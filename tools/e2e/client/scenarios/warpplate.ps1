# U3 warp plate, real client: place two plates by hand, wait for both to attune and eject their
# shards, pick up plate A's shard, insert it into plate B, walk onto B and get warped to A.
# Screenshots: out/warpplate/{attuning,ejected,items,idle,warping,arrived}.png
$placedRx = 'warp plate placed: (.+) \((\w*)\) at (-?\d+) (-?\d+) (-?\d+) by (.+)$'
function Get-Plate([string]$line) {
    if ($line -notmatch $placedRx) { throw "unexpected placement line: $line" }
    [pscustomobject]@{ Name = $Matches[1]; X = [int]$Matches[3]; Y = [int]$Matches[4]; Z = [int]$Matches[5] }
}
function Esc([string]$s) { [regex]::Escape($s) }

$r = Invoke-SelfTest 'warpplate.*'
if ($r.Pass -lt 11) { throw "expected 11 warpplate selftests to pass, got $($r.Pass)" }

Invoke-Chat '/gamemode creative'
Invoke-Chat '/clear @s'
Invoke-Chat '/replaceitem entity @s slot.hotbar 0 bedstones:warp_plate 2'
Press-Key '1'

# Plate A: look at the ground block two blocks south of the player and right-click it.
Invoke-Chat '/tp @s 10 -60 10 facing 10.5 -61.62 12.5'
Start-Sleep -Milliseconds 800
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
$a = Get-Plate (Wait-LogLine 'warp plate placed: .* by (?!\?)' 15 -Since $mark)
Wait-LogLine "warp plate status: $(Esc $a.Name) attuning" 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'attuning' | Out-Null
Wait-LogLine "warp plate shard ejected: attuned shard from $(Esc $a.Name)" 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 600
Save-Screenshot 'ejected' | Out-Null

# Plate B, six blocks east.
Invoke-Chat '/tp @s 16 -60 10 facing 16.5 -61.62 12.5'
Start-Sleep -Milliseconds 800
$markB = Get-LogMark
Press-Key '1'
Click 0.5 0.5 -Relative -Right
$b = Get-Plate (Wait-LogLine 'warp plate placed: .* by (?!\?)' 15 -Since $markB)
Wait-LogLine "warp plate shard ejected: attuned shard from $(Esc $b.Name)" 10 -Since $markB | Out-Null

# Pick up A's shard: it lies on plate A (empty now, so standing there is safe).
Invoke-Chat "/tp @s $($a.X + 0.5) $($a.Y + 0.2) $($a.Z + 0.5)"
Start-Sleep -Milliseconds 1500
Open-Inventory
Save-Screenshot 'items' | Out-Null
Close-Screen
# The shard went to the first free hotbar slot (2; slot 1 holds the plates).
Press-Key '2'
Invoke-Chat "/tp @s $($b.X) $($b.Y) $($b.Z - 3) facing $($b.X + 0.5) $($b.Y - 1.52) $($b.Z + 0.5)"
Start-Sleep -Milliseconds 800
$markI = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine "warp plate shard inserted: bedstones:attuned_shard into $(Esc $b.Name)" 10 -Since $markI | Out-Null
Wait-LogLine "warp plate status: $(Esc $b.Name) idle" 10 -Since $markI | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'idle' | Out-Null

# Step onto B and get warped to A.
$markW = Get-LogMark
# (Land on top of the plate: a player put inside its 1-pixel collision box is pushed aside.)
Invoke-Chat "/tp @s $($b.X + 0.5) $($b.Y + 0.2) $($b.Z + 0.5) facing $($b.X + 0.5) $($b.Y - 4) $($b.Z + 2.5)"
Wait-LogLine "warp plate status: $(Esc $b.Name) warping" 10 -Since $markW | Out-Null
Start-Sleep -Milliseconds 1000   # use time is 100 ticks here (prelaunch config)
Save-Screenshot 'warping' | Out-Null
Wait-LogLine "warp plate warp: (?!minecraft:).* from $(Esc $b.Name) to $(Esc $a.Name)" 15 -Since $markW | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'arrived' | Out-Null
