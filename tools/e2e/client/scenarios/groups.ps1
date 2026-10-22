# U12 Groups, client: the native Manage Groups and Edit Group screens driven headlessly.
#   /waystones debug groups -> Manage Groups; "+" -> Edit Group; type a name; cycle the icon (left and
#   right click) and the colour; Save; hide an inbuilt group; drag-reorder; delete a user group with
#   Shift held. Every step is checked against the log lines of the server (and of the client screens).
# Screenshots: out/groups/{manage,edit_new,edit_filled,manage_created,hidden,dragging,dragged,delete_shift,deleted,closed}.png
$r = Invoke-SelfTest 'groups.*'
if ($r.Pass -lt 7) { throw "expected at least 7 groups selftests to pass, got $($r.Pass)" }

Wait-LogLine 'companion: server ready' 30 | Out-Null

# GUI layout -> client-area fractions. The screens log their layout (GUI units) once when shown.
$script:Gui = $null
function Read-Layout([string]$screen, [int]$mark) {
    $line = Wait-LogLine "groups: $screen layout screen=" 15 -Since $mark
    if ("$line" -notmatch 'screen=([\d.]+)x([\d.]+) left=([-\d.]+) top=([-\d.]+)') { throw "no layout in '$line'" }
    $script:Gui = [pscustomobject]@{ W = [double]$Matches[1]; H = [double]$Matches[2]; Left = [double]$Matches[3]; Top = [double]$Matches[4] }
    Write-Host "layout ${screen}: $($script:Gui | ConvertTo-Json -Compress)"
    $script:Gui
}
function GuiClick([double]$x, [double]$y, [switch]$Right) { Click ($x / $script:Gui.W) ($y / $script:Gui.H) -Relative -Right:$Right }
function GuiMove([double]$x, [double]$y) { Move-Mouse ($x / $script:Gui.W) ($y / $script:Gui.H) -Relative }
# Manage Groups geometry (ManageGroupsScreen.cpp): list x = left + 25, rows from top + 64, 22 high;
# in a row: drag handle at +2..+18, edit at +189, delete / hide at +205 (13x13, from row y + 4).
function ListX { $script:Gui.Left + 25 }
function RowY([int]$i) { $script:Gui.Top + 64 + 22 * $i + 10.5 }
function LastX { (ListX) + 205 + 6.5 }

# ---- open Manage Groups -------------------------------------------------------------------------
$mark = Get-LogMark
Invoke-Chat '/waystones debug groups'
Wait-LogLine 'groups: manage_groups opened for' 15 -Since $mark | Out-Null
Wait-LogLine 'companion: screen manage_groups shown' 15 -Since $mark | Out-Null
$m = Read-Layout 'manage_groups' $mark
Start-Sleep -Milliseconds 500
Save-Screenshot 'manage' | Out-Null

# ---- "+" -> Edit Group -----------------------------------------------------------------------------
$mark = Get-LogMark
GuiClick ((ListX) + 210) ($m.Top + 50)
Wait-LogLine 'groups: edit_group opened \(new\)' 15 -Since $mark | Out-Null
Wait-LogLine 'companion: screen edit_group shown' 15 -Since $mark | Out-Null
$e = Read-Layout 'edit_group' $mark
Start-Sleep -Milliseconds 400
Save-Screenshot 'edit_new' | Out-Null

# Name (the field has focus when the screen opens; click it anyway).
GuiClick ($e.Left + 60) ($e.Top + 46)
Type-Text 'Farms'
# Icon: left click twice (community_hubs -> dimension -> dungeons), right click once (-> dimension).
$mark = Get-LogMark
$iconX = $e.Left + 132 + 10.5; $btnY = $e.Top + 35.5 + 10.5
GuiClick $iconX $btnY
GuiClick $iconX $btnY
GuiClick $iconX $btnY -Right
Wait-LogLine 'groups: edit_group icon -> dimension' 10 -Since $mark | Out-Null
$icons = @(Get-LeviLog -Since $mark | Where-Object { $_ -match 'edit_group icon -> ' } | ForEach-Object { ($_ -replace '.*icon -> ', '').Trim() })
if (($icons -join ',') -ne 'dimension,dungeons,dimension') { throw "icon cycle was '$($icons -join ',')'" }
# Colour: white -> gold -> light_purple -> aqua -> yellow -> green.
$colorX = $e.Left + 155 + 10.5
for ($i = 0; $i -lt 5; $i++) { GuiClick $colorX $btnY }
Wait-LogLine 'groups: edit_group color -> green' 10 -Since $mark | Out-Null
GuiMove $iconX $btnY
Start-Sleep -Milliseconds 400
Save-Screenshot 'edit_filled' | Out-Null

# Save.
$mark = Get-LogMark
GuiClick ($e.Left + 88) ($e.Top + 106)
$created = Wait-LogLine "groups: created group waystones:user/[0-9a-f-]+ 'Farms' icon=dimension color=green" 15 -Since $mark
$userId = ([regex]::Match("$created", 'waystones:user/[0-9a-f-]+')).Value
Wait-LogLine 'groups: manage_groups opened for' 15 -Since $mark | Out-Null
$m = Read-Layout 'manage_groups' $mark
Start-Sleep -Milliseconds 500
Save-Screenshot 'manage_created' | Out-Null

# ---- hide an inbuilt group (row 2 = Global Waystones) ---------------------------------------------
$mark = Get-LogMark
GuiClick (LastX) (RowY 2)
Wait-LogLine 'groups: hid inbuilt group waystones:global by' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'hidden' | Out-Null

# ---- drag the new group (last row, 7) up to row 5 ----------------------------------------------------
$mark = Get-LogMark
$hx = (ListX) + 10
GuiMove $hx (RowY 7)
[BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 200
Wait-LogLine "groups: client started dragging $([regex]::Escape($userId))" 10 -Since $mark | Out-Null
GuiMove $hx (RowY 6)
Start-Sleep -Milliseconds 300
GuiMove $hx (RowY 5)
Start-Sleep -Milliseconds 300
Save-Screenshot 'dragging' | Out-Null
[BsE2E.Native]::MouseButton(0, $true); Start-Sleep -Milliseconds 300
Wait-LogLine "groups: moved $([regex]::Escape($userId)) to 5 by" 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'dragged' | Out-Null

# ---- delete it (now row 5): without Shift nothing happens, with Shift it is deleted ----------------
$mark = Get-LogMark
GuiClick (LastX) (RowY 5)
Start-Sleep -Milliseconds 800
if (Get-LeviLog -Since $mark | Where-Object { $_ -match 'groups: deleted group' }) { throw 'delete without Shift must do nothing' }
GuiMove (LastX) (RowY 5)
[BsE2E.Native]::Key(0x10, $false); Start-Sleep -Milliseconds 300
Save-Screenshot 'delete_shift' | Out-Null
[BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::MouseButton(0, $true)
Start-Sleep -Milliseconds 300
[BsE2E.Native]::Key(0x10, $true)
Wait-LogLine "groups: deleted group $([regex]::Escape($userId)) by" 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'deleted' | Out-Null

# Unhide Global Waystones again, then Back (no source: closes the screen).
$mark = Get-LogMark
GuiClick (LastX) (RowY 2)
Wait-LogLine 'groups: unhid inbuilt group waystones:global by' 10 -Since $mark | Out-Null
$mark = Get-LogMark
GuiClick ($m.Left - 8 + 10) ($m.Top + 50)
Wait-LogLine 'groups: .* left manage_groups' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 600
Save-Screenshot 'closed' | Out-Null
