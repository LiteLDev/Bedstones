# U11 Manage Waystones + Personal Settings, driven through the real client:
# seed four activated waystones -> open the manage screen -> drag row 1 below row 2 (swap) ->
# double-click a handle (move to top) -> shift+click delete -> personal settings: alias,
# favorite, save -> back on the manage screen.
# Screenshots: out/manage/{manage,dragged,moved_top,delete_hover,deleted,personal_settings,personal_filled,manage_after}.png

Wait-LogLine 'companion: server ready' 60 | Out-Null
$mark = Get-LogMark
Invoke-Chat '/waystones debug manage seed'
Wait-LogLine 'manage: seeded 4 waystones for' 15 -Since $mark | Out-Null

$mark = Get-LogMark
Invoke-Chat '/waystones debug manage'
Wait-LogLine 'companion: screen manage shown' 15 -Since $mark | Out-Null
$layout = Wait-LogLine 'manage screen: layout gui=' 10 -Since $mark
if ($layout -notmatch 'gui=([\d.]+)x([\d.]+) left=([\d.-]+) top=([\d.-]+) rows=(\d+)') { throw "unexpected layout line: $layout" }
$gw = [double]$Matches[1]; $gh = [double]$Matches[2]; $left = [double]$Matches[3]; $top = [double]$Matches[4]
if ([int]$Matches[5] -lt 4) { throw "expected at least 4 rows, got $($Matches[5])" }
Write-Host "[manage] gui ${gw}x${gh} left=$left top=$top"

# GUI coordinates -> fractions of the client area.
function GuiX([double]$x) { $x / $gw }
function GuiY([double]$y) { $y / $gh }
$listX = $left + 25
function RowY([int]$i) { $top + 64 + 22 * $i }
function HandleAt([int]$i) { @((GuiX ($listX + 8)), (GuiY ((RowY $i) + 11))) }
function EditAt([int]$i) { @((GuiX ($listX + 190 + 6)), (GuiY ((RowY $i) + 4 + 6))) }
function ActionAt([int]$i) { @((GuiX ($listX + 205 + 6)), (GuiY ((RowY $i) + 4 + 6))) }

Start-Sleep -Milliseconds 500
Save-Screenshot 'manage' | Out-Null

# Drag "Alpha Peak" (row 0) by its handle down to row 1: one swap step.
$mark = Get-LogMark
$h = HandleAt 0
Move-Mouse $h[0] $h[1] -Relative
[BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 150
foreach ($dy in 4, 8, 12, 16, 20, 24) {
    Move-Mouse $h[0] (GuiY ((RowY 0) + 11 + $dy)) -Relative
}
Start-Sleep -Milliseconds 200
[BsE2E.Native]::MouseButton(0, $true); Start-Sleep -Milliseconds 300
Wait-LogLine "manage: swapped 'Alpha Peak' <-> 'Birch Hollow' by" 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'dragged' | Out-Null

# Double-click the handle of "Cedar Ford" (row 2): move to the top.
$mark = Get-LogMark
$h = HandleAt 2
Move-Mouse $h[0] $h[1] -Relative
foreach ($n in 1, 2) { [BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 40; [BsE2E.Native]::MouseButton(0, $true); Start-Sleep -Milliseconds 60 }
Wait-LogLine "manage: moved 'Cedar Ford' to the top by" 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'moved_top' | Out-Null
# Order now: Cedar Ford, Birch Hollow, Alpha Peak, Dune Gate.

# Shift + click the delete X of "Dune Gate" (row 3).
$mark = Get-LogMark
$a = ActionAt 3
Move-Mouse $a[0] $a[1] -Relative
[BsE2E.Native]::Key(0x10, $false); Start-Sleep -Milliseconds 300
Move-Mouse $a[0] $a[1] -Relative
Save-Screenshot 'delete_hover' | Out-Null
[BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::MouseButton(0, $true)
Start-Sleep -Milliseconds 200
[BsE2E.Native]::Key(0x10, $true)
Wait-LogLine "manage: deleted 'Dune Gate' \(forget\) by" 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'deleted' | Out-Null

# Personal settings of "Alpha Peak" (row 2) through its pencil icon.
$mark = Get-LogMark
$e = EditAt 2
Click $e[0] $e[1] -Relative
Wait-LogLine "personal settings: opened 'Alpha Peak' for" 10 -Since $mark | Out-Null
Wait-LogLine 'companion: screen personal_settings shown' 10 -Since $mark | Out-Null
$ps = Wait-LogLine 'personal settings screen: layout gui=' 10 -Since $mark
if ($ps -notmatch 'left=([\d.-]+) top=([\d.-]+)') { throw "unexpected layout line: $ps" }
$pl = [double]$Matches[1]; $pt = [double]$Matches[2]
Start-Sleep -Milliseconds 500
Save-Screenshot 'personal_settings' | Out-Null

Click (GuiX ($pl + 60)) (GuiY ($pt + 69)) -Relative
Type-Text 'Summit'
Click (GuiX ($pl + 10)) (GuiY ($pt + 122)) -Relative
Start-Sleep -Milliseconds 300
Save-Screenshot 'personal_filled' | Out-Null

$mark = Get-LogMark
Click (GuiX ($pl + 88)) (GuiY ($pt + 152)) -Relative
Wait-LogLine "personal settings: saved 'Alpha Peak' by .* \(alias 'Summit', group '', favorite true\)" 10 -Since $mark | Out-Null
Wait-LogLine 'companion: screen manage shown' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 600
Save-Screenshot 'manage_after' | Out-Null

Close-Screen
$mark = Get-LogMark
Invoke-Chat '/waystones debug manage dump'
$order = Wait-LogLine 'manage: order for .*: ' 10 -Since $mark
if ($order -notmatch 'Cedar Ford, Birch Hollow, Summit') { throw "unexpected final order: $order" }
if ($order -match 'Dune Gate') { throw "deleted waystone still listed: $order" }
