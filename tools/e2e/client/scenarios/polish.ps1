# Faithfulness details in the real client:
#   1. scrolls in hand: first person idle / charging (pulled towards the centre, trembling), third
#      person idle / charging (attachables + animations, rp/attachables/*.json)
#   2. warp plate name in the Standard Galactic glyphs (font page E1) in the Warp Modifiers screen
#   3. activated runes glowing at midnight (additive rune particles)
# Screenshots: out/polish/*.png

Wait-LogLine 'companion: server ready' 60 | Out-Null
Invoke-Chat '/gamerule dodaylightcycle false'
Invoke-Chat '/time set noon'
Invoke-Chat '/weather clear'

function Snap([string]$name, [int]$settleMs = 1200) { Start-Sleep -Milliseconds $settleMs; Save-Screenshot $name | Out-Null }
function Layout-Of([string]$line) {
    if ($line -notmatch 'gui=([\d.]+)x([\d.]+) left=([\d.-]+) top=([\d.-]+)') { throw "no layout in '$line'" }
    [pscustomobject]@{ W = [double]$Matches[1]; H = [double]$Matches[2]; L = [double]$Matches[3]; T = [double]$Matches[4] }
}

# ---- 1. scrolls in hand ------------------------------------------------------------------------------
Invoke-Chat '/gamemode survival'
Invoke-Chat '/clear @s'
Invoke-Chat '/give @s bedstones:warp_scroll 3'
Invoke-Chat '/give @s bedstones:warp_stone 1'
Press-Key '1'
Invoke-Chat '/tp @s 0.5 -60 0.5 0 0'
Snap 'scroll_idle_fp'
$mark = Get-LogMark
[BsE2E.Native]::MouseButton(1, $false)
Wait-LogLine 'scroll charging: bedstones:warp_scroll by' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 400                   # the scroll takes 32 ticks: still charging
Save-Screenshot 'scroll_charge_fp' | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'scroll_charge_fp2' | Out-Null
[BsE2E.Native]::MouseButton(1, $true)          # released before the 32 ticks: the charge is cancelled
Start-Sleep -Milliseconds 800
if (Get-LeviLog -Since $mark | Select-String 'companion: screen selection shown') { Close-Screen }
Press-Key '2'
Snap 'stone_idle_fp'

Press-Key '1'
Press-Key 'F5'                                  # third person (back)
Snap 'scroll_idle_tp' 1500
[BsE2E.Native]::MouseButton(1, $false)
Start-Sleep -Milliseconds 900
Save-Screenshot 'scroll_charge_tp' | Out-Null
[BsE2E.Native]::MouseButton(1, $true)
Start-Sleep -Milliseconds 600
Press-Key 'F5'
Press-Key 'F5'                                  # back to first person
Start-Sleep -Milliseconds 600

# ---- 2. galactic warp plate name -----------------------------------------------------------------------
Invoke-Chat '/gamemode creative'
Invoke-Chat '/clear @s'
Invoke-Chat '/replaceitem entity @s slot.hotbar 0 bedstones:warp_plate 1'
Press-Key '1'
Invoke-Chat '/tp @s 10 -60 10 facing 10.5 -61.62 12.5'
Start-Sleep -Seconds 2
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'warp plate placed: .* by (?!\?)' 15 -Since $mark | Out-Null
Start-Sleep -Seconds 3                          # attunes and ejects its shard
Invoke-Chat '/clear @s'
Start-Sleep -Milliseconds 500
$mark = Get-LogMark
Click 0.5 0.5 -Relative -Right
Wait-LogLine 'companion: screen edit shown' 15 -Since $mark | Out-Null
$edit = Layout-Of (Wait-LogLine 'edit screen: layout' 10 -Since $mark)
Start-Sleep -Milliseconds 500
Click (($edit.L + 10) / $edit.W) (($edit.T + 130) / $edit.H) -Relative   # Manage Modifiers
Wait-LogLine 'companion: screen modifiers shown' 15 -Since $mark | Out-Null
Snap 'modifiers_galactic' 800
Close-Screen
Start-Sleep -Milliseconds 800

# ---- 3. runes glowing at midnight --------------------------------------------------------------------------
Invoke-Chat '/gamemode survival'
Invoke-Chat '/waystones visuals clear'
Invoke-Chat '/tp @s 0.5 -60 -20.5 0 0'
Start-Sleep -Seconds 1
$mark = Get-LogMark
Invoke-Chat '/waystones visuals place waystone'
Invoke-Chat '/tp @s ~2 ~ ~'
Invoke-Chat '/waystones visuals place blackstone_waystone'
Invoke-Chat '/tp @s ~-4 ~ ~'
Invoke-Chat '/waystones visuals place end_stone_waystone'
Wait-LogLine 'visuals: placed end_stone_waystone' 10 -Since $mark | Out-Null
Invoke-Chat '/waystones visuals activate'
Wait-LogLine 'visuals: client has 3 waystones \(3 with runes\)' 15 -Since $mark | Out-Null
Invoke-Chat '/tp @s ~2 ~0.5 ~-1 0 12'
Snap 'runes_noon' 2000
Invoke-Chat '/time set midnight'
Snap 'runes_midnight' 2500
Invoke-Chat '/time set noon'
Invoke-Chat '/waystones visuals clear'
