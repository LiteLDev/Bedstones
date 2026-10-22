# Inventory button (U14): the "Return to Waystone" button over the vanilla inventory.
# Mode NEAREST + a debug waystone -> open the inventory -> button at the configured position ->
# hover tooltip -> click -> confirm screen -> Yes -> warp; then on cooldown: 50% alpha sprite,
# cooldown tooltip, a click only plays the click sound.
# Screenshots: out/inventorybutton/{inventory,tooltip,confirm,after_warp,cooldown,cooldown_tooltip}.png

Wait-LogLine 'companion: server ready' 60 | Out-Null

$mark = Get-LogMark
Invoke-Chat '/waystones inventorybutton mode NEAREST'
Wait-LogLine "inventory button mode set to 'NEAREST'" 15 -Since $mark | Out-Null
Invoke-Chat '/waystones inventorybutton debug waystone "IB Home"'
Wait-LogLine 'inventory button debug: waystone IB Home at .* activated' 15 -Since $mark | Out-Null
# Without the requirements unit nothing has a cooldown: use the test requirements (300 s).
Invoke-Chat '/waystones inventorybutton debug cooldowns'
Wait-LogLine 'inventory button debug: test requirements on' 15 -Since $mark | Out-Null
Wait-LogLine 'inventory button: mode nearest' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500

function Get-ButtonGeometry([int]$Since) {
    $line = Wait-LogLine 'inventory button: shown at ([\d.]+),([\d.]+) on (\S+) \(([\d.]+)x([\d.]+)\)' 15 -Since $Since
    if ($line -notmatch 'shown at ([\d.]+),([\d.]+) on (\S+) \(([\d.]+)x([\d.]+)\)') { throw "bad geometry line: $line" }
    [pscustomobject]@{ X = [double]$Matches[1]; Y = [double]$Matches[2]; Screen = $Matches[3]; W = [double]$Matches[4]; H = [double]$Matches[5] }
}

# ---- the button --------------------------------------------------------------------------------------
$mark = Get-LogMark
Move-Mouse 0.05 0.05 -Relative
Open-Inventory
$g = Get-ButtonGeometry $mark
Write-Step "button at $($g.X),$($g.Y) on $($g.Screen), GUI $($g.W)x$($g.H)"
Start-Sleep -Milliseconds 500
Save-Screenshot 'inventory' | Out-Null

$bx = ($g.X + 8) / $g.W
$by = ($g.Y + 8) / $g.H
Move-Mouse $bx $by -Relative
Wait-LogLine 'inventory button: tooltip shown' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'tooltip' | Out-Null

# ---- confirm + warp -----------------------------------------------------------------------------------
Click $bx $by -Relative
Wait-LogLine 'inventory button: clicked, confirming IB Home' 10 -Since $mark | Out-Null
Wait-LogLine 'companion: screen inventory_confirm shown' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'confirm' | Out-Null

# Yes: (width/2 - 155, height/6 + 96, 150x20).
$yesX = ([math]::Floor($g.W / 2) - 155 + 75) / $g.W
$yesY = ([math]::Floor($g.H / 6) + 96 + 10) / $g.H
Click $yesX $yesY -Relative
Wait-LogLine 'inventory button: confirmed warp to IB Home' 10 -Since $mark | Out-Null
Wait-LogLine 'inventory button: .* warped to IB Home' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'after_warp' | Out-Null
Close-Screen
Start-Sleep -Milliseconds 1500

# ---- on cooldown --------------------------------------------------------------------------------------
$mark = Get-LogMark
Move-Mouse 0.05 0.05 -Relative
Open-Inventory
$g = Get-ButtonGeometry $mark
Start-Sleep -Milliseconds 500
Save-Screenshot 'cooldown' | Out-Null
$bx = ($g.X + 8) / $g.W
$by = ($g.Y + 8) / $g.H
Move-Mouse $bx $by -Relative
Wait-LogLine 'inventory button: tooltip shown' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'cooldown_tooltip' | Out-Null
Click $bx $by -Relative
Wait-LogLine 'inventory button: clicked while unaffordable' 10 -Since $mark | Out-Null
Close-Screen

Invoke-Chat '/waystones inventorybutton debug cooldowns'
Start-Sleep -Milliseconds 1500

# ---- creative inventory ----------------------------------------------------------------------------------
Invoke-Chat '/gamemode creative'
Start-Sleep -Milliseconds 1500
$mark = Get-LogMark
Move-Mouse 0.05 0.05 -Relative
Open-Inventory
Start-Sleep -Milliseconds 2500
# The item catalog: no survival inventory panel, so no button.
Save-Screenshot 'creative' | Out-Null
if (Get-LeviLog -Since $mark | Where-Object { $_ -match 'inventory button: shown at' }) { throw 'button shown over the creative item catalog' }
# The survival-inventory (crafting table) tab, top right: the button is back.
Click (905 / 1280) (137 / 720) -Relative
$g = Get-ButtonGeometry $mark
Start-Sleep -Milliseconds 500
Move-Mouse (($g.X + 8) / $g.W) (($g.Y + 8) / $g.H) -Relative
Wait-LogLine 'inventory button: tooltip shown' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'creative_survival_tab' | Out-Null
Close-Screen
Invoke-Chat '/gamemode survival'
Invoke-Chat '/waystones inventorybutton mode NONE'
