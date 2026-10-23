# U8 requirements on a real client: XP is deducted from the player's bar when warping, and the
# inventory-button cooldown is started, synced to the client mod and blocks the next warp.
# Screenshots: out/requirements/{xp_before,xp_after,xp_level30_before,xp_level30_after,cooldown}.png
Wait-LogLine 'companion: server ready' 60 | Out-Null
# The cooldown sync after the handshake reaches the client mod.
Wait-LogLine 'requirements: client received \d+ cooldown' 30 | Out-Null

$mark = Get-LogMark
Invoke-Chat '/gamemode s'
Invoke-Chat '/waystones debug requirements reset'
Wait-LogLine 'requirements debug: cooldowns reset' 15 -Since $mark | Out-Null
Invoke-Chat '/xp -10000L @s'
Invoke-Chat '/xp 5L @s'
Invoke-Chat '/waystones debug requirements show'
Wait-LogLine 'requirements debug: .* level 5, 55 xp points' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'xp_before' | Out-Null

# 2700 blocks in the same dimension: 0.01 points per block = 27 points (the cap).
$mark = Get-LogMark
Invoke-Chat '/waystones debug requirements warp 2700'
Wait-LogLine 'requirements: .* paid 27 xp points' 20 -Since $mark | Out-Null
Wait-LogLine 'requirements debug warp: .* cost 27 xp -> ok \(xp 55 -> 28\)' 20 -Since $mark | Out-Null
Start-Sleep -Seconds 3
Invoke-Chat '/waystones debug requirements show'
Wait-LogLine 'requirements debug: .* level 3, 28 xp points' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'xp_after' | Out-Null

# Within one level: level 30 + 100 points, 1550 blocks cost 15 -> level 30 + 85 points (the bar shrinks).
$mark = Get-LogMark
Invoke-Chat '/xp -10000L @s'
Invoke-Chat '/xp 30L @s'
Invoke-Chat '/xp 100 @s'
Invoke-Chat '/waystones debug requirements show'
Wait-LogLine 'requirements debug: .* level 30, 1495 xp points' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800
Save-Screenshot 'xp_level30_before' | Out-Null
$mark = Get-LogMark
Invoke-Chat '/waystones debug requirements warp 1550'
Wait-LogLine 'requirements debug warp: .* cost 15 xp -> ok \(xp 1495 -> 1480\)' 20 -Since $mark | Out-Null
Start-Sleep -Seconds 3
Save-Screenshot 'xp_level30_after' | Out-Null

# Inventory button source: 300 s cooldown, synced to the client, then the next one is refused.
$mark = Get-LogMark
Invoke-Chat '/waystones debug requirements warp 10 inventory_button'
Wait-LogLine 'requirements: cooldown waystones:inventory_button started' 20 -Since $mark | Out-Null
Wait-LogLine 'requirements: client received 1 cooldown' 20 -Since $mark | Out-Null
$mark = Get-LogMark
Invoke-Chat '/waystones debug requirements warp 10 inventory_button'
Wait-LogLine 'requirements debug warp: .* source inventory_button .* -> failed' 20 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'cooldown' | Out-Null

# Creative: the inventory button resets the cooldown instead of being blocked.
$mark = Get-LogMark
Invoke-Chat '/gamemode c'
Invoke-Chat '/waystones debug requirements warp 10 inventory_button'
Wait-LogLine 'requirements: creative .* reset 1 cooldown' 20 -Since $mark | Out-Null
Wait-LogLine 'requirements: client received 0 cooldown' 20 -Since $mark | Out-Null
