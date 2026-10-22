# U1 waystone blocks in the real client (BS-Test: flat, grass top at y -61, survival):
#   1. place a waystone with a right-click: both halves, facing the player; the Edit screen opens
#   2. a waystone set by a command (no record yet) is activated by a right-click (chat message)
#   3. the next right-click opens the selection menu
#   4. breaking it with a pickaxe removes both halves and the record
# Screenshots: out/waystone/{edit_after_place,placed,placed_side,activated,selection,broken}.png
Wait-LogLine 'companion: server ready' 60 | Out-Null
Invoke-Chat '/gamemode survival'
Invoke-Chat '/clear @s'
Invoke-Chat '/give @s bedstones:waystone 1'
Invoke-Chat '/give @s diamond_pickaxe 1'
Press-Key '1'

# 1. Place: stand at (8.5, -60, 8.5) looking south and down at the grass one block ahead.
Invoke-Chat '/tp @s 8.5 -60 8.5 0 60'
Start-Sleep -Seconds 2
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
$placed = Wait-LogLine 'waystone placed: .* at 8 -60 9 facing 2 by' 15 -Since $mark
Write-Step "placed: $placed"
Wait-LogLine 'waystone edit opened: ' 10 -Since $mark | Out-Null
Start-Sleep -Seconds 2
Save-Screenshot 'edit_after_place' | Out-Null
Close-Screen
Start-Sleep -Milliseconds 500
# Step back and look at it: the front (rune) side faces the player (north).
Invoke-Chat '/tp @s 8.5 -60 5.5 0 15'
Start-Sleep -Seconds 2
Save-Screenshot 'placed' | Out-Null
Invoke-Chat '/tp @s 5.5 -60 9.5 -90 15'
Start-Sleep -Seconds 2
Save-Screenshot 'placed_side' | Out-Null

# 2. Activate a waystone that has no record yet (as if generated): it gets a name on first use.
Invoke-Chat '/setblock 12 -60 9 bedstones:sandy_waystone ["bedstones:facing"=2,"bedstones:upper"=0]'
Invoke-Chat '/setblock 12 -59 9 bedstones:sandy_waystone ["bedstones:facing"=2,"bedstones:upper"=1]'
Invoke-Chat '/tp @s 12.5 -60 6.5 0 20'
Start-Sleep -Seconds 2
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
$activated = Wait-LogLine 'waystone activated: .+ by ' 15 -Since $mark
Write-Step "activated: $activated"
Start-Sleep -Milliseconds 800
Save-Screenshot 'activated' | Out-Null

# 3. Right-click again: the selection menu (form or native screen).
Start-Sleep -Seconds 1
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'waystone selection opened: .+ by ' 15 -Since $mark | Out-Null
Start-Sleep -Seconds 2
Save-Screenshot 'selection' | Out-Null
Close-Screen
Start-Sleep -Milliseconds 500

# 4. Break it in survival with the diamond pickaxe (hold the left button).
Press-Key '2'
Start-Sleep -Milliseconds 300
$mark = Get-LogMark
[BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Seconds 4; [BsE2E.Native]::MouseButton(0, $true)
$removed = Wait-LogLine 'waystone removed: .+ by ' 15 -Since $mark
Write-Step "removed: $removed"
Start-Sleep -Seconds 1
Save-Screenshot 'broken' | Out-Null
