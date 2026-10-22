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

# Runs $Do until the log shows $Pattern (headless input occasionally drops a press).
function Invoke-Step([scriptblock]$Do, [string]$Pattern, [int]$Attempts = 3) {
    for ($i = 1; $i -le $Attempts; $i++) {
        $m = Get-LogMark
        & $Do
        try { return Wait-LogLine $Pattern 5 -Since $m } catch { Write-Host "[manage] step not seen (attempt $i): $Pattern" }
    }
    throw "step failed after $Attempts attempts: $Pattern"
}

Start-Sleep -Milliseconds 500
Save-Screenshot 'manage' | Out-Null

# Drag "Alpha Peak" (row 0) by its handle down to row 1: one swap step.
Invoke-Step {
    $h = HandleAt 0
    Move-Mouse $h[0] $h[1] -Relative
    Start-Sleep -Milliseconds 200
    [BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 150
    foreach ($dy in 4, 8, 12, 16, 20, 24) { Move-Mouse $h[0] (GuiY ((RowY 0) + 11 + $dy)) -Relative }
    Start-Sleep -Milliseconds 200
    [BsE2E.Native]::MouseButton(0, $true); Start-Sleep -Milliseconds 300
} "manage: swapped 'Alpha Peak' <-> 'Birch Hollow' by" | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'dragged' | Out-Null

# Double-click the handle of "Cedar Ford" (row 2): move to the top.
Invoke-Step {
    $h = HandleAt 2
    Move-Mouse $h[0] $h[1] -Relative
    Start-Sleep -Milliseconds 200
    foreach ($n in 1, 2) { [BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 40; [BsE2E.Native]::MouseButton(0, $true); Start-Sleep -Milliseconds 60 }
    Start-Sleep -Milliseconds 300
} "manage: moved 'Cedar Ford' to the top by" | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'moved_top' | Out-Null
# Order now: Cedar Ford, Birch Hollow, Alpha Peak, Dune Gate.

# Shift + click the delete X of "Dune Gate" (row 3).
$a = ActionAt 3
Move-Mouse $a[0] $a[1] -Relative
[BsE2E.Native]::Key(0x10, $false); Start-Sleep -Milliseconds 300
Move-Mouse $a[0] $a[1] -Relative
Save-Screenshot 'delete_hover' | Out-Null
Invoke-Step {
    Move-Mouse $a[0] $a[1] -Relative
    [BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 60; [BsE2E.Native]::MouseButton(0, $true)
    Start-Sleep -Milliseconds 200
} "manage: deleted 'Dune Gate' \(forget\) by" | Out-Null
[BsE2E.Native]::Key(0x10, $true)
Start-Sleep -Milliseconds 500
Save-Screenshot 'deleted' | Out-Null

# Personal settings of "Alpha Peak" (row 2) through its pencil icon.
$mark = Get-LogMark
$e = EditAt 2
Invoke-Step { Click $e[0] $e[1] -Relative } "personal settings: opened 'Alpha Peak' for" | Out-Null
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
Invoke-Step { Click (GuiX ($pl + 88)) (GuiY ($pt + 152)) -Relative } "personal settings: saved 'Alpha Peak' by .* \(alias 'Summit', group '', favorite true\)" | Out-Null
Wait-LogLine 'companion: screen manage shown' 10 -Since $mark | Out-Null
Start-Sleep -Milliseconds 600
Save-Screenshot 'manage_after' | Out-Null

Close-Screen
$mark = Get-LogMark
Invoke-Chat '/waystones debug manage dump'
$order = Wait-LogLine 'manage: order for .*: ' 10 -Since $mark
if ($order -notmatch 'Cedar Ford, Birch Hollow, Summit') { throw "unexpected final order: $order" }
if ($order -match 'Dune Gate') { throw "deleted waystone still listed: $order" }
