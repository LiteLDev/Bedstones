# Integration: the whole Waystones loop in the real client with every unit installed, driven only
# through the game (blocks placed by right-click, native screens clicked), no debug shortcuts:
#   1. place waystone A (right-click) -> Edit screen opens -> name it "Home Alpha" -> Save
#   2. place waystone B 300 blocks east the same way -> "Beta Point"
#   3. right-click B (activated by placing) -> selection screen -> click "Home Alpha" -> warp, XP paid
#   4. right-click A -> selection -> Manage Waystones -> double-click "Beta Point" to the top
#   5. Personal Settings of "Beta Point": alias "Far Beta" + Favorite -> Save
#   6. inventory button (mode NEAREST): confirm -> warp; a second use is on the 300 s cooldown
# Screenshots: out/fullflow/*.png

Wait-LogLine 'companion: server ready' 60 | Out-Null
Invoke-Chat '/gamemode survival'
Invoke-Chat '/clear @s'
Invoke-Chat '/xp 30L @s'
Invoke-Chat '/give @s bedstones:waystone 2'
Press-Key '1'

# Runs $Do until the log shows $Pattern (headless input occasionally drops a press).
function Invoke-Step([scriptblock]$Do, [string]$Pattern, [int]$Attempts = 3, [int]$Seconds = 6) {
    for ($i = 1; $i -le $Attempts; $i++) {
        $m = Get-LogMark
        & $Do
        try { return Wait-LogLine $Pattern $Seconds -Since $m } catch { Write-Step "step not seen (attempt $i): $Pattern" }
    }
    throw "step failed after $Attempts attempts: $Pattern"
}
function Layout-Of([string]$line) {
    if ($line -notmatch 'gui=([\d.]+)x([\d.]+) left=([\d.-]+) top=([\d.-]+)') { throw "no layout in '$line'" }
    [pscustomobject]@{ W = [double]$Matches[1]; H = [double]$Matches[2]; L = [double]$Matches[3]; T = [double]$Matches[4] }
}
function Click-At($lay, [double]$x, [double]$y) { Click (($lay.L + $x) / $lay.W) (($lay.T + $y) / $lay.H) -Relative }

# Places a waystone on the grass ahead (looking south and down) and names it in the Edit screen.
function Place-And-Name([double]$x, [string]$name) {
    Invoke-Chat "/tp @s $x -60 8.5 0 60"
    Start-Sleep -Seconds 2
    $m = Get-LogMark
    Invoke-Step { Click 0.5 0.5 -Relative -Right } "waystone placed: .* at $([math]::Floor($x)) -60 9 facing 2 by" | Out-Null
    Wait-LogLine 'companion: screen edit shown' 15 -Since $m | Out-Null
    $edit = Layout-Of (Wait-LogLine 'edit screen: layout' 10 -Since $m)
    Start-Sleep -Milliseconds 500
    Click-At $edit 75 79                       # name field
    Type-Text $name
    Start-Sleep -Milliseconds 300
    Save-Screenshot "edit_$($name -replace ' ', '_')" | Out-Null
    Invoke-Step { Click-At $edit 88 160 } "edit: waystone saved $name \(" | Out-Null   # Save
    Start-Sleep -Milliseconds 800
}

# 1 + 2: two waystones, named through the native Edit screen.
Place-And-Name 8.5 'Home Alpha'
Place-And-Name 308.5 'Beta Point'

# Selection screen helpers (layout line of U10's screen).
function Open-SelectionAt([double]$x) {
    Invoke-Chat "/tp @s $x -60 6.5 0 20"
    Start-Sleep -Seconds 2
    $m = Get-LogMark
    Invoke-Step { Click 0.5 0.5 -Relative -Right } 'waystone selection opened: .+ by' | Out-Null
    Wait-LogLine 'companion: screen selection shown' 15 -Since $m | Out-Null
    $line = Wait-LogLine 'selection screen: layout ' 10 -Since $m
    if ($line -notmatch 'screen=([\d.]+)x([\d.]+) panel=([-\d.]+),([-\d.]+),270,([\d.]+) .* list=([-\d.]+),([-\d.]+)') { throw "unparsable layout: $line" }
    $lay = [pscustomobject]@{ W = [double]$Matches[1]; H = [double]$Matches[2]; Left = [double]$Matches[3]; Top = [double]$Matches[4]; ListX = [double]$Matches[6]; ListY = [double]$Matches[7] }
    Start-Sleep -Milliseconds 600
    return $lay
}

# 3: at B, open the selection by right-clicking it and warp to A; the XP cost is paid.
$m = Get-LogMark
$sel = Open-SelectionAt 308.5
$rowsLine = @(Get-LeviLog -Since $m | Where-Object { $_ -match 'selection screen: rows=' }) | Select-Object -Last 1
$null = $rowsLine -match 'rows=(.*) sort='
$rows = @($Matches[1] -split '\|')
Write-Step "selection rows: $($rows -join ', ')"
Save-Screenshot 'selection_at_beta' | Out-Null
$i = [array]::IndexOf($rows, 'Home Alpha')
if ($i -lt 0) { throw "Home Alpha not listed: $($rows -join ', ')" }
$m = Get-LogMark
Click (($sel.ListX + 110) / $sel.W) (($sel.ListY + 22 * $i + 11) / $sel.H) -Relative
Wait-LogLine 'warp: .* -> Home Alpha' 20 -Since $m | Out-Null
$paid = Wait-LogLine 'requirements: .* paid \d+ xp points' 10 -Since $m
Write-Step "paid: $paid"
Start-Sleep -Seconds 2
Save-Screenshot 'arrived_home' | Out-Null

# 4: at A, selection -> Manage Waystones (left column, y + 40).
$sel = Open-SelectionAt 8.5
$m = Get-LogMark
Click (($sel.Left - 8 + 10) / $sel.W) (($sel.Top + 40 + 10) / $sel.H) -Relative
Wait-LogLine 'companion: screen manage shown' 15 -Since $m | Out-Null
$ml = Wait-LogLine 'manage screen: layout gui=' 10 -Since $m
$man = Layout-Of $ml
Start-Sleep -Milliseconds 600
Save-Screenshot 'manage' | Out-Null
$listX = $man.L + 25
function RowY([int]$r) { $man.T + 64 + 22 * $r }
# Beta Point is row 1 (sorting index: A, then B): double-click its handle -> top.
Invoke-Step {
    Move-Mouse (($listX + 8) / $man.W) (((RowY 1) + 11) / $man.H) -Relative
    Start-Sleep -Milliseconds 200
    foreach ($n in 1, 2) { [BsE2E.Native]::MouseButton(0, $false); Start-Sleep -Milliseconds 40; [BsE2E.Native]::MouseButton(0, $true); Start-Sleep -Milliseconds 60 }
    Start-Sleep -Milliseconds 300
} "manage: moved 'Beta Point' to the top by" | Out-Null
Start-Sleep -Milliseconds 500
Save-Screenshot 'manage_reordered' | Out-Null

# 5: Personal Settings of Beta Point (now row 0): alias + favorite, Save.
$m = Get-LogMark
Invoke-Step { Click (($listX + 196) / $man.W) (((RowY 0) + 10) / $man.H) -Relative } "personal settings: opened 'Beta Point' for" | Out-Null
Wait-LogLine 'companion: screen personal_settings shown' 10 -Since $m | Out-Null
$ps = Layout-Of (Wait-LogLine 'personal settings screen: layout gui=' 10 -Since $m)
Start-Sleep -Milliseconds 500
Click-At $ps 60 69
Type-Text 'Far Beta'
Click-At $ps 10 122
Start-Sleep -Milliseconds 300
Save-Screenshot 'personal_settings' | Out-Null
Invoke-Step { Click-At $ps 88 152 } "personal settings: saved 'Beta Point' by .* \(alias 'Far Beta', group '', favorite true\)" | Out-Null
Start-Sleep -Milliseconds 600
Save-Screenshot 'manage_after' | Out-Null
Close-Screen
Start-Sleep -Seconds 1

# 6: inventory button (NEAREST) from 40 blocks away: confirm -> warp, then the cooldown.
$m = Get-LogMark
Invoke-Chat '/waystones inventorybutton mode NEAREST'
Wait-LogLine "inventory button mode set to 'NEAREST'" 15 -Since $m | Out-Null
Invoke-Chat '/tp @s 48.5 -60 8.5 90 0'
Start-Sleep -Seconds 3
$m = Get-LogMark
Move-Mouse 0.05 0.05 -Relative
Open-Inventory
$line = Wait-LogLine 'inventory button: shown at ([\d.]+),([\d.]+) on (\S+) \(([\d.]+)x([\d.]+)\)' 15 -Since $m
$null = $line -match 'shown at ([\d.]+),([\d.]+) on (\S+) \(([\d.]+)x([\d.]+)\)'
$gx = [double]$Matches[1]; $gy = [double]$Matches[2]; $gw = [double]$Matches[4]; $gh = [double]$Matches[5]
$bx = ($gx + 8) / $gw; $by = ($gy + 8) / $gh
Move-Mouse $bx $by -Relative
Start-Sleep -Milliseconds 500
Save-Screenshot 'inventory_button' | Out-Null
Click $bx $by -Relative
Wait-LogLine 'companion: screen inventory_confirm shown' 10 -Since $m | Out-Null
Start-Sleep -Milliseconds 400
Save-Screenshot 'inventory_confirm' | Out-Null
Click (([math]::Floor($gw / 2) - 155 + 75) / $gw) (([math]::Floor($gh / 6) + 96 + 10) / $gh) -Relative
Wait-LogLine 'inventory button: .* warped to Home Alpha' 15 -Since $m | Out-Null
Wait-LogLine 'requirements: cooldown waystones:inventory_button started' 10 -Since $m | Out-Null
Start-Sleep -Seconds 2
Save-Screenshot 'inventory_warped' | Out-Null
Close-Screen                                   # the inventory stays open after the warp
Start-Sleep -Seconds 1

# Again: on cooldown now (U8's default rule: 300 s).
$m = Get-LogMark
Open-Inventory
$null = Wait-LogLine 'inventory button: shown at' 15 -Since $m
Move-Mouse $bx $by -Relative
Start-Sleep -Milliseconds 600
Save-Screenshot 'inventory_cooldown' | Out-Null
Click $bx $by -Relative
Wait-LogLine 'inventory button: clicked while unaffordable' 10 -Since $m | Out-Null
Close-Screen
Invoke-Chat '/waystones inventorybutton mode NONE'
