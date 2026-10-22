# U13: Edit Waystone screen and Warp Modifiers screen in the real client.
#   /waystones debug edit -> edit screen (screenshot) -> type a name, cycle the visibility
#   (screenshot) -> Save (log) -> reopen -> Manage Modifiers -> modifiers screen (screenshot) ->
#   shift-click a diamond from the hotbar into it (log, screenshot).
# Screenshots: out/edit/{edit_open,edit_typed,modifiers,modifiers_diamond}.png

Wait-LogLine 'companion: server ready' 60 | Out-Null
$r = Invoke-SelfTest 'edit.*'
if ($r.Pass -lt 5) { throw "expected at least 5 edit selftests to pass, got $($r.Pass)" }
$r = Invoke-SelfTest 'modifiers.*'
if ($r.Pass -lt 4) { throw "expected at least 4 modifiers selftests to pass, got $($r.Pass)" }

# GUI layout logged by the screens: "gui=WxH left=L top=T".
function Get-Layout([string]$line) {
    if ($line -notmatch 'gui=([\d.]+)x([\d.]+) left=([\d.-]+) top=([\d.-]+)') { throw "no layout in '$line'" }
    [pscustomobject]@{ W = [double]$Matches[1]; H = [double]$Matches[2]; L = [double]$Matches[3]; T = [double]$Matches[4] }
}
# Clicks GUI point (L + x, T + y) of a layout.
function Click-Gui($layout, [double]$x, [double]$y, [switch]$Right) {
    Click (($layout.L + $x) / $layout.W) (($layout.T + $y) / $layout.H) -Relative -Right:$Right
}

Invoke-Chat '/clear @s'
Invoke-Chat '/give @s diamond 3'

# ---- edit screen --------------------------------------------------------------------------------
$mark = Get-LogMark
Invoke-Chat '/waystones debug edit'
Wait-LogLine 'edit: debug waystone .* created' 15 -Since $mark | Out-Null
Wait-LogLine 'companion: screen edit shown' 15 -Since $mark | Out-Null
$edit = Get-Layout (Wait-LogLine 'edit screen: layout' 10 -Since $mark)
Start-Sleep -Milliseconds 500
Save-Screenshot 'edit_open' | Out-Null

Click-Gui $edit 75 79                 # the name field
Type-Text 'Bedstones Test'
Click-Gui $edit 10 105                # visibility icon button: next option
Wait-LogLine 'edit screen: visibility -> ' 10 -Since $mark | Out-Null
Move-Mouse (($edit.L + 120) / $edit.W) (($edit.T + 30) / $edit.H) -Relative
Start-Sleep -Milliseconds 400
Save-Screenshot 'edit_typed' | Out-Null
Click-Gui $edit 88 160                # Save
Wait-LogLine 'edit: waystone saved Bedstones Test \(\w+\) by ' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 800

# ---- modifiers screen -------------------------------------------------------------------------------
$mark = Get-LogMark
Invoke-Chat '/waystones debug edit'   # same spot: reopens the waystone
Wait-LogLine 'companion: screen edit shown' 15 -Since $mark | Out-Null
$edit = Get-Layout (Wait-LogLine 'edit screen: layout' 10 -Since $mark)
Start-Sleep -Milliseconds 400
Click-Gui $edit 10 130                # Manage Modifiers
Wait-LogLine 'companion: screen modifiers shown' 15 -Since $mark | Out-Null
$mods = Get-Layout (Wait-LogLine 'modifiers screen: layout' 10 -Since $mark)
Start-Sleep -Milliseconds 500
Save-Screenshot 'modifiers' | Out-Null

# Shift-click the diamonds in hotbar slot 0 (8, 162): they skip the shard slot and land in slot 1.
[BsE2E.Native]::Key([uint16]0x10, $false); Start-Sleep -Milliseconds 150
Click-Gui $mods 16 170
[BsE2E.Native]::Key([uint16]0x10, $true); Start-Sleep -Milliseconds 150
Wait-LogLine 'modifiers: quick_move slot 5 by .* -> \[_, minecraft:diamond x3' 15 -Since $mark | Out-Null
Move-Mouse (($mods.L + 88) / $mods.W) (($mods.T + 30) / $mods.H) -Relative
Start-Sleep -Milliseconds 600
Save-Screenshot 'modifiers_diamond' | Out-Null

# Hover the diamond for its tooltip, then close with Esc.
Move-Mouse (($mods.L + 88) / $mods.W) (($mods.T + 25) / $mods.H) -Relative
Start-Sleep -Milliseconds 400
Save-Screenshot 'modifiers_tooltip' | Out-Null
Close-Screen
Start-Sleep -Milliseconds 600
Save-Screenshot 'closed' | Out-Null
