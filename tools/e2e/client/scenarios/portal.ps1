# U5 client scenario: Epitaph -> Fleeting Memorial, Portal Scroll -> Warp Portal -> warp.
#
# 1. /kill with an epitaph: a Fleeting Memorial (with the owner's name label) appears where the
#    player died; after respawning it is listed in the selection menu (/waystones debug memorials).
# 2. A Portal Scroll is charged (right mouse button held), the memorial is picked in the selection
#    menu: a Warp Portal opens ahead; walking into it warps to the memorial, which then disappears.
# Screenshots: out/portal/{memorial,memorial_listed,scroll_selection,portal,warped}.png

Wait-LogLine 'companion: server ready' 60 | Out-Null
Invoke-Chat '/gamerule doimmediaterespawn true'
Invoke-Chat '/gamemode survival'
Invoke-Chat '/clear @s'
Invoke-Chat '/time set noon'

# ---- Epitaph: die, respawn, see the memorial ------------------------------------------------------
$mark = Get-LogMark
Invoke-Chat '/give @s bedstones:epitaph'
Invoke-Chat '/kill @s'
$re = 'fleeting memorial placed: (\S+) for .* at (-?\d+) (-?\d+) (-?\d+) facing (\d)'
$placed = Wait-LogLine $re 20 -Since $mark
if ($placed -notmatch $re) { throw "unexpected memorial line: $placed" }
$memUid = $Matches[1]; $mx = [int]$Matches[2]; $my = [int]$Matches[3]; $mz = [int]$Matches[4]; $facing = [int]$Matches[5]
Write-Step "memorial $memUid at $mx $my $mz facing $facing"
Start-Sleep -Seconds 3
# Look at the memorial's front from 3 blocks away (Java 2D facing: 0 S, 1 W, 2 N, 3 E).
$off = @(@(0, 1), @(-1, 0), @(0, -1), @(1, 0))[$facing]
Invoke-Chat "/tp @s $($mx + 3 * $off[0] + 0.5) $($my - 1) $($mz + 3 * $off[1] + 0.5) facing $($mx + 0.5) $($my - 1.5) $($mz + 0.5)"
Start-Sleep -Seconds 2
Save-Screenshot 'memorial' | Out-Null
# Side view (the slab is one pixel thin).
Invoke-Chat "/tp @s $($mx + 2 * $off[1] + 2 * $off[0] + 0.5) $($my - 1) $($mz - 2 * $off[0] + 2 * $off[1] + 0.5) facing $($mx + 0.5) $($my - 1.5) $($mz + 0.5)"
Start-Sleep -Seconds 1
Save-Screenshot 'memorial_side' | Out-Null

$mark = Get-LogMark
Invoke-Chat '/waystones debug memorials'
Wait-LogLine "memorial debug: listed .*\($memUid\)" 10 -Since $mark | Out-Null
Wait-LogLine 'memorial debug: 1 memorial\(s\) listed' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'memorial_listed' | Out-Null
Close-Screen

# ---- Portal Scroll: charge, pick the memorial, walk through the portal --------------------------
$px = $mx + 12
Invoke-Chat "/tp @s $px $($my - 1) $mz -90 0"
Invoke-Chat '/give @s bedstones:portal_scroll 2'
Press-Key '1'
Start-Sleep -Seconds 1
$mark = Get-LogMark
Focus-Game
[BsE2E.Native]::MouseButton(1, $false); Start-Sleep -Milliseconds 2600; [BsE2E.Native]::MouseButton(1, $true)
Wait-LogLine 'portal scroll charged by' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'scroll_selection' | Out-Null
# The selection menu lists the memorial (pinned, first button).
Click 0.5 0.36 -Relative
$opened = Wait-LogLine "warp portal opened: (\S+) -> $memUid by " 10 -Since $mark
Start-Sleep -Seconds 2
Save-Screenshot 'portal' | Out-Null

$mark = Get-LogMark
Press-Key W 900
Wait-LogLine 'warp portal used: .* -> ' 10 -Since $mark | Out-Null
Wait-LogLine "fleeting memorial used: $memUid" 10 -Since $mark | Out-Null
Start-Sleep -Seconds 2
Save-Screenshot 'warped' | Out-Null
