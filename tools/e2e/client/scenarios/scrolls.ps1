# U4 scrolls in the real client (BS-Test flat world, survival):
#   1. warp scroll: hold right-click 2 s (charge particles), the selection opens, pick a waystone,
#      the warp happens and one scroll is consumed;
#   2. warp stone: same, loses 1 durability;
#   3. blank scroll (tooltip) used on waystone A's block -> bound scroll -> hold -> warps to A, consumed;
#   4. return scroll -> nearest activated waystone.
# Screenshots: out/scrolls/{items,charging,selection,warped,warped_count,warped_inventory,stone_charging,
#   stone_warped,blank_tooltip,bind_target,bound,bound_charging,bound_warped,return_warped}.png
Wait-LogLine 'companion: server ready' 60 | Out-Null

# Selection list (default seam = vanilla form; U10's native screen when present): the first entry.
$FirstEntryX = 0.5
$FirstEntryY = 0.38

function Hold-Use([int]$Ms, [string]$Shot) {
    [BsE2E.Native]::MouseButton(1, $false)
    Start-Sleep -Milliseconds ([Math]::Min($Ms, 900))
    if ($Shot) { Save-Screenshot $Shot | Out-Null }
    Start-Sleep -Milliseconds ([Math]::Max(0, $Ms - 900))
    [BsE2E.Native]::MouseButton(1, $true)
}

Invoke-Chat '/gamemode survival'
Invoke-Chat '/tp @s 0 -60 0 facing 0 -59 -20'
Invoke-Chat '/clear @s'
$mark = Get-LogMark
Invoke-Chat '/waystones debug scrolls'
$setup = Wait-LogLine 'scrolls debug: A=(\S+) at (-?\d+) (-?\d+) (-?\d+), B=(\S+)' 15 -Since $mark
if ("$setup" -notmatch 'A=(\S+) at (-?\d+) (-?\d+) (-?\d+), B=(\S+)') { throw "no setup line: $setup" }
$aUid = $Matches[1]; $ax = [int]$Matches[2]; $ay = [int]$Matches[3]; $az = [int]$Matches[4]; $bUid = $Matches[5]
Write-Step "waystone A $aUid at $ax $ay $az, B $bUid"

# ---- 1. warp scroll -------------------------------------------------------------------------------
Invoke-Chat '/give @s bedstones:warp_scroll 3'
Press-Key '1'
Start-Sleep -Milliseconds 500
Save-Screenshot 'items' | Out-Null
$mark = Get-LogMark
Hold-Use 2000 'charging'
Wait-LogLine 'scroll charging: bedstones:warp_scroll' 5 -Since $mark | Out-Null
Wait-LogLine 'scroll charged: bedstones:warp_scroll by .*, opening selection' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'selection' | Out-Null
Select-SelectionRow -Since $mark -Index 0 -FormY $FirstEntryY
Wait-LogLine "selection: .* selected .* \(($aUid|$bUid)\)" 10 -Since $mark | Out-Null
Wait-LogLine 'warp: .* -> ' 10 -Since $mark | Out-Null
Wait-LogLine 'scroll consumed: bedstones:warp_scroll by .* \(left 2\)' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1200
Save-Screenshot 'warped' | Out-Null
Invoke-Chat '/clear @s bedstones:warp_scroll 0 0'
Start-Sleep -Milliseconds 500
Save-Screenshot 'warped_count' | Out-Null   # chat: "has 2 items", hotbar count 2
Open-Inventory
Save-Screenshot 'warped_inventory' | Out-Null
Close-Screen

# ---- 2. warp stone --------------------------------------------------------------------------------
Invoke-Chat '/clear @s'
Invoke-Chat '/tp @s 0 -60 0 facing 0 -59 -20'
Invoke-Chat '/give @s bedstones:warp_stone 1'
Press-Key '1'
$mark = Get-LogMark
Hold-Use 2000 'stone_charging'
Wait-LogLine 'scroll charged: bedstones:warp_stone by .*, opening selection' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Select-SelectionRow -Since $mark -Index 0 -FormY $FirstEntryY
Wait-LogLine 'warp stone damaged: 1/128' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'stone_warped' | Out-Null

# ---- 3. blank -> bound scroll ---------------------------------------------------------------------
Invoke-Chat '/clear @s'
Invoke-Chat '/give @s bedstones:blank_scroll 1'
Press-Key '1'
# The client mod adds Waystones' tooltip line to the blank scroll (hotbar slot 1 in the inventory).
Open-Inventory
Move-Mouse 0.5055 0.728 -Relative
Start-Sleep -Milliseconds 800
Save-Screenshot 'blank_tooltip' | Out-Null
Close-Screen
# Waystone A's real block (placed by /waystones debug scrolls): stand two blocks north of it and
# look at its lower half.
Invoke-Chat ("/tp @s {0}.5 {1} {2}.5 facing {0}.5 {3}.5 {4}.5" -f $ax, $ay, ($az - 2), $ay, $az)
Start-Sleep -Milliseconds 800
Save-Screenshot 'bind_target' | Out-Null
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'scroll bound: Scroll Test A' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 600
Save-Screenshot 'bound' | Out-Null
Invoke-Chat ("/tp @s {0} {1} {2} facing {0} {3} {4}" -f ($ax - 20), $ay, ($az - 20), ($ay + 1), ($az - 40))
Start-Sleep -Milliseconds 1500   # away from waystone A first (a right-click on it would open its menu)
$mark = Get-LogMark
Hold-Use 2000 'bound_charging'
Wait-LogLine "scroll charged: bedstones:bound_scroll by .*, warping to $aUid" 10 -Since $mark | Out-Null
Wait-LogLine 'scroll consumed: bedstones:bound_scroll by .* \(left 0\)' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1200
Save-Screenshot 'bound_warped' | Out-Null

# ---- 4. return scroll -----------------------------------------------------------------------------
Invoke-Chat '/clear @s'
Invoke-Chat ("/tp @s {0} {1} {2} facing {0} {3} {4}" -f ($ax + 30), $ay, $az, ($ay + 1), ($az - 20))
Invoke-Chat '/give @s bedstones:return_scroll 2'
Press-Key '1'
$mark = Get-LogMark
Hold-Use 2000 ''
Wait-LogLine "scroll charged: bedstones:return_scroll by .*, warping to $aUid" 10 -Since $mark | Out-Null
Wait-LogLine 'scroll consumed: bedstones:return_scroll by .* \(left 1\)' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1200
Save-Screenshot 'return_warped' | Out-Null
