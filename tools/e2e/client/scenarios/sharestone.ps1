# U2: sharestones and portstones in a real client (local world).
# Places two red sharestones and a white portstone with right-clicks, screenshots them, right-clicks
# the second sharestone (selection lists the first one), picks it (the player warps) and right-clicks
# the portstone. Screenshots: out/sharestone/*.png
$r = Invoke-SelfTest 'sharestone.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 5) { throw "sharestone selftests: pass=$($r.Pass) fail=$($r.Fail)" }
$r = Invoke-SelfTest 'portstone.*'
if ($r.Fail -gt 0 -or $r.Pass -lt 2) { throw "portstone selftests: pass=$($r.Pass) fail=$($r.Fail)" }
Wait-LogLine 'companion: server ready' 30 | Out-Null

function Get-Uid([string]$line) { [regex]::Match($line, '\(([0-9a-f-]{36})\)').Groups[1].Value }

Invoke-Chat '/gamemode creative'
Invoke-Chat '/clear @s'
Invoke-Chat '/replaceitem entity @s slot.hotbar 0 bedstones:red_sharestone'
Invoke-Chat '/replaceitem entity @s slot.hotbar 1 bedstones:white_portstone'
Invoke-Chat '/time set noon'
# Look north and down at the ground right in front of the player.
Invoke-Chat '/tp @s ~ ~ ~ 180 60'
Press-Key '1'
Start-Sleep -Milliseconds 600
Save-Screenshot 'aim' | Out-Null

# 1. First red sharestone: placed facing the player, record created, Edit opens (form without U13).
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
$lineA = Wait-LogLine 'sharestone placed: red_sharestone at .*' 10 -Since $mark
$uidA = Get-Uid $lineA
Start-Sleep -Milliseconds 1500
Save-Screenshot 'edit-after-place' | Out-Null
Close-Screen

# 2. Second red sharestone three blocks east.
Invoke-Chat '/tp @s ~3 ~ ~ 180 60'
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
$lineB = Wait-LogLine 'sharestone placed: red_sharestone at .*' 10 -Since $mark
$uidB = Get-Uid $lineB
if ($uidA -eq $uidB) { throw "both placements reported the same record" }
Start-Sleep -Milliseconds 1500
Close-Screen

# 3. White portstone three more blocks east.
Invoke-Chat '/tp @s ~3 ~ ~ 180 60'
Press-Key '2'
Start-Sleep -Milliseconds 400
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'portstone placed: white at .*' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800

# Their look, from the front and from the side.
Invoke-Chat '/tp @s ~-3 ~1 ~4 180 15'
Start-Sleep -Milliseconds 1500
Save-Screenshot 'stones-front' | Out-Null
Invoke-Chat '/tp @s ~4 ~0 ~-3 90 15'
Start-Sleep -Milliseconds 1500
Save-Screenshot 'stones-side' | Out-Null
Invoke-Chat '/tp @s ~-4 ~-1 ~-1 180 0'
Press-Key '3'
Start-Sleep -Milliseconds 400

# 4. Right-click the second sharestone with an empty hand: the selection lists the first one.
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'sharestone used: .* by ' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'selection' | Out-Null
# The selection lists the other sharestone (the clicked one is shown as the current, disabled row).
# Rows are in manual order: sharestone A (placed first) is row 0.
Select-SelectionRow -Since $mark -Index 0 -FormY 0.355
Wait-LogLine "selection: .* selected .* \($uidA\)" 10 -Since $mark | Out-Null
$warp = Wait-LogLine 'warp: .* -> ' 10 -Since $mark
Start-Sleep -Milliseconds 1500
Save-Screenshot 'warped' | Out-Null

# 5. Right-click the portstone (6 blocks east of the first sharestone).
Invoke-Chat '/tp @s ~6 ~ ~ 180 20'
Start-Sleep -Milliseconds 800
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'portstone used: white by ' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'portstone-selection' | Out-Null
Close-Screen

# 6. Item tooltips in the inventory (hover the hotbar slots of the creative inventory).
Invoke-Chat '/replaceitem entity @s slot.hotbar 2 bedstones:blue_portstone'
Open-Inventory
Save-Screenshot 'inventory' | Out-Null
Move-Mouse 0.366 0.803 -Relative
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip-sharestone' | Out-Null
Move-Mouse 0.432 0.803 -Relative
Start-Sleep -Milliseconds 800
Save-Screenshot 'tooltip-portstone' | Out-Null
Close-Screen
Assert-NoErrors
