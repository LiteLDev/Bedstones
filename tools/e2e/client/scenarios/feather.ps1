# Twinbound Feather (U6) on the real client: the linking ritual with a simulated partner, the
# heart particles, the partner entry in a selection menu, warping to it and the feather lore.
# Screenshots: out/feather/{partner,ritual,linked,lore,menu,warped,inventory}.png
$r = Invoke-SelfTest 'feather.*'
if ($r.Pass -lt 7) { throw "expected 7 feather selftests to pass, got $($r.Pass)" }
Wait-LogLine 'companion: server ready' 30 | Out-Null

# Face south, level, with a feather in hand; the partner spawns 1.5 blocks in front, facing us.
Invoke-Chat '/tp @s ~ ~ ~ 0 0'
Invoke-Chat '/replaceitem entity @s slot.weapon.mainhand 0 bedstones:twinbound_feather'
$mark = Get-LogMark
Invoke-Chat '/waystones debug feather partner'
Wait-LogLine 'feather: debug partner Twin spawned' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'partner' | Out-Null

# Hold use (right mouse button) while the partner holds use too: 20 ticks later they link.
$mark = Get-LogMark
Focus-Game
[BsE2E.Native]::MouseButton(1, $false)
try {
    Start-Sleep -Milliseconds 700
    Save-Screenshot 'ritual' | Out-Null
    Wait-LogLine 'feather linked: .+ <-> Twin|feather linked: Twin <-> .+' 15 -Since $mark | Out-Null
    Start-Sleep -Milliseconds 150
    Save-Screenshot 'linked' | Out-Null
} finally {
    [BsE2E.Native]::MouseButton(1, $true)
}

# Re-select the slot so the item popup shows the name and the "Linked to" lore.
Start-Sleep -Milliseconds 800
Press-Key 2
Press-Key 1
Start-Sleep -Milliseconds 300
Save-Screenshot 'lore' | Out-Null

# The selection menu (inventory-button source) lists the partner.
$mark = Get-LogMark
Invoke-Chat '/waystones debug feather menu'
Wait-LogLine 'feather: debug menu for .+ \(1 partner' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'menu' | Out-Null
# Pick the partner: the warp lands at Twin.
$mark = Get-LogMark
Click 0.5 0.343 -Relative
Wait-LogLine 'feather warp: .+ -> player:' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'warped' | Out-Null

# The linked feather's tooltip in the inventory ("Linked to: Twin").
Open-Inventory
Move-Mouse 0.506 0.726 -Relative   # hotbar slot 1 of the survival inventory
Start-Sleep -Milliseconds 600
Save-Screenshot 'inventory' | Out-Null
Close-Screen

Invoke-Chat '/waystones debug feather clear'
