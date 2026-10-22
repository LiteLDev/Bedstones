# U10 selection screen on the real client: layout against docs/SPEC.md §4.1, search, sort,
# group filter, requirement tooltips, current-location hover, select -> warp, admin variant.
# Screenshots: out/selection/*.png
Wait-LogLine 'companion: server ready' 60 | Out-Null

Invoke-Chat '/gamemode survival'
Invoke-Chat '/xp 10L @s'
$mark = Get-LogMark
Invoke-Chat '/waystones debug selection setup'
Wait-LogLine 'selection debug: created \d+ waystones' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones debug selection costs'
Wait-LogLine 'selection debug: requirement preview on' 15 -Since $mark | Out-Null

# ---- open + layout ----------------------------------------------------------------------------
function Open-Selection([string]$cmd = '/waystones debug selection', [string]$id = 'selection') {
    $m = Get-LogMark
    Invoke-Chat $cmd
    Wait-LogLine "companion: screen $id shown" 20 -Since $m | Out-Null
    $line = Wait-LogLine 'selection screen: layout ' 10 -Since $m
    Start-Sleep -Milliseconds 600
    return (Get-Layout $line)
}

function Get-Layout([string]$line) {
    if ($line -notmatch 'screen=([\d.]+)x([\d.]+) panel=([-\d.]+),([-\d.]+),270,([\d.]+) title_y=([-\d.]+) search=([-\d.]+),([-\d.]+),([\d.]+),([\d.]+) sort=(\S+) manage=(\S+) list=([-\d.]+),([-\d.]+) groups=(\S+)') {
        throw "unparsable layout line: $line"
    }
    $L = [pscustomobject]@{
        W = [double]$Matches[1]; H = [double]$Matches[2]; Left = [double]$Matches[3]; Top = [double]$Matches[4]; PanelH = [double]$Matches[5]
        TitleY = [double]$Matches[6]; SearchX = [double]$Matches[7]; SearchY = [double]$Matches[8]; SearchW = [double]$Matches[9]; SearchH = [double]$Matches[10]
        Sort = $Matches[11]; Manage = $Matches[12]; ListX = [double]$Matches[13]; ListY = [double]$Matches[14]; Groups = $Matches[15]
    }
    return $L
}

function Get-Rows([int]$since) {
    $line = @(Get-LeviLog -Since $since | Where-Object { $_ -match 'selection screen: rows=' }) | Select-Object -Last 1
    if (-not $line) { throw 'no rows line' }
    $null = $line -match 'rows=(.*) sort='
    return @($Matches[1] -split '\|')
}

function Gui([object]$L, [double]$x, [double]$y) { return @(($x / $L.W), ($y / $L.H)) }
function Click-Gui([object]$L, [double]$x, [double]$y, [switch]$Right) { $p = Gui $L $x $y; Click $p[0] $p[1] -Relative -Right:$Right }
function Hover-Gui([object]$L, [double]$x, [double]$y) { $p = Gui $L $x $y; Move-Mouse $p[0] $p[1] -Relative }
function Assert-Near([double]$actual, [double]$expected, [string]$what) {
    if ([Math]::Abs($actual - $expected) -gt 0.01) { throw "layout: $what is $actual, expected $expected" }
}

$rowsMark = Get-LogMark
$L = Open-Selection
$rows = Get-Rows $rowsMark
Write-Step "selection layout: $($L | ConvertTo-Json -Compress)"
Write-Step "rows: $($rows -join ', ')"
Save-Screenshot 'open' | Out-Null

# Layout against the spec (Java GUI coordinates).
$n = $rows.Count
$expectedH = [Math]::Min([Math]::Max(200, 64 + 22 * $n), [Math]::Floor($L.H) - 40)
Assert-Near $L.PanelH $expectedH 'panel height'
Assert-Near $L.Left ([Math]::Floor(($L.W - 270) / 2)) 'leftPos'
Assert-Near $L.Top ([Math]::Floor(($L.H - $L.PanelH) / 2)) 'topPos'
Assert-Near $L.TitleY $L.Top 'title y (source waystone present)'
Assert-Near $L.SearchX ($L.Left + 25) 'search x'; Assert-Near $L.SearchY ($L.Top + 40) 'search y'
Assert-Near $L.SearchW 198 'search width (with sort button)'; Assert-Near $L.SearchH 20 'search height'
if ($L.Sort -ne "$($L.Left + 225),$($L.Top + 40)") { throw "layout: sort button at $($L.Sort)" }
if ($L.Manage -ne "$($L.Left - 8),$($L.Top + 40),inactive") { throw "layout: manage button $($L.Manage) (expected inactive without the manage unit)" }
Assert-Near $L.ListX ($L.Left + 25) 'list x'; Assert-Near $L.ListY ($L.Top + 64) 'list y'
$firstGroup = ($L.Groups -split ';')[0]
if ($firstGroup -notmatch "^waystones:favorites@$($L.Left - 8),$($L.Top + 65)$") { throw "layout: first group button $firstGroup" }

# ---- search -----------------------------------------------------------------------------------
$m = Get-LogMark
Click-Gui $L ($L.SearchX + 60) ($L.SearchY + 10)
Type-Text 'mine'
Wait-LogLine "selection screen: search 'mine' -> 1 rows" 10 -Since $m | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'search' | Out-Null
Click-Gui $L ($L.SearchX + 60) ($L.SearchY + 10) -Right   # right click clears
Wait-LogLine "selection screen: search '' -> $n rows" 10 -Since $m | Out-Null
Click-Gui $L ($L.Left + 135) ($L.Top + 30)              # unfocus (empty header area)

# ---- sort ---------------------------------------------------------------------------------------
$sx = $L.Left + 225 + 10; $sy = $L.Top + 40 + 10
$m = Get-LogMark
Click-Gui $L $sx $sy
Wait-LogLine 'selection: .* sort mode name' 10 -Since $m | Out-Null
Wait-LogLine 'selection screen: rows=.* sort=name' 10 -Since $m | Out-Null
Hover-Gui $L $sx $sy
Start-Sleep -Milliseconds 300
Save-Screenshot 'sort_name' | Out-Null
$m = Get-LogMark
Click-Gui $L $sx $sy
Wait-LogLine 'selection screen: rows=.* sort=distance' 10 -Since $m | Out-Null
$rows = Get-Rows $m
Start-Sleep -Milliseconds 300
Save-Screenshot 'sort_distance' | Out-Null
$expectedDistance = @('Spawn Hearth', 'Alpha Village', 'Market Square', 'brook crossing', 'Forbidden Keep', 'Cooldown Camp', 'Iron Mine', 'Far Lighthouse', 'Nether Hub', 'End Gate')
if (($rows -join '|') -ne ($expectedDistance -join '|')) { throw "distance order: $($rows -join ', ')" }

# ---- requirement tooltips (distance order) ----------------------------------------------------
function Row-Y([string]$name) {
    $i = [array]::IndexOf($rows, $name)
    if ($i -lt 0) { throw "row $name not listed" }
    return $L.ListY + 22 * $i + 1 + 10
}
$reqX = $L.ListX + 2 + 2 + 8
Hover-Gui $L $reqX (Row-Y 'Alpha Village'); Start-Sleep -Milliseconds 400
Save-Screenshot 'tooltip_xp' | Out-Null
Hover-Gui $L $reqX (Row-Y 'Cooldown Camp'); Start-Sleep -Milliseconds 400
Save-Screenshot 'tooltip_cooldown' | Out-Null
Hover-Gui $L $reqX (Row-Y 'Iron Mine'); Start-Sleep -Milliseconds 400
Save-Screenshot 'tooltip_item' | Out-Null
Hover-Gui $L $reqX (Row-Y 'Forbidden Keep'); Start-Sleep -Milliseconds 400
Save-Screenshot 'tooltip_refuse' | Out-Null

# ---- group filter -------------------------------------------------------------------------------
$m = Get-LogMark
Click-Gui $L ($L.Left - 8 + 10) ($L.Top + 65 + 10)
Wait-LogLine "selection screen: filter 'waystones:favorites'" 10 -Since $m | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'filter_favorites' | Out-Null
Click-Gui $L ($L.Left - 8 + 10) ($L.Top + 65 + 10)
Wait-LogLine "selection screen: filter ''" 10 -Since $m | Out-Null

# ---- current location hover -----------------------------------------------------------------------
Hover-Gui $L ($L.Left + 135 + 30) ($L.Top + 24); Start-Sleep -Milliseconds 400
Save-Screenshot 'current_hover' | Out-Null

# Back to manual so the next run starts from the default.
$m = Get-LogMark
Click-Gui $L $sx $sy
Wait-LogLine 'selection screen: rows=.* sort=manual' 10 -Since $m | Out-Null
$rows = Get-Rows $m

# ---- select -> warp -----------------------------------------------------------------------------
$m = Get-LogMark
Click-Gui $L ($L.ListX + 2 + 108) (Row-Y 'Alpha Village')
Wait-LogLine 'selection: .* selected Alpha Village' 10 -Since $m | Out-Null
Wait-LogLine 'warp: .* -> ' 10 -Since $m | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'after_warp' | Out-Null

# ---- admin variant ------------------------------------------------------------------------------
$A = Open-Selection '/waystones debug selection admin' 'admin_selection'
if ($A.Sort -ne 'none' -or $A.Manage -ne 'none') { throw "admin view shows sort ($($A.Sort)) / manage ($($A.Manage))" }
Assert-Near $A.SearchW 220 'admin search width'
Assert-Near $A.TitleY ($A.Top + 20) 'admin title y (no source)'
Save-Screenshot 'admin' | Out-Null
Close-Screen

# ---- edit current -------------------------------------------------------------------------------
$L = Open-Selection
$m = Get-LogMark
Click-Gui $L ($L.Left + 135 + 30) ($L.Top + 24)
Wait-LogLine 'selection: .* edit_current' 10 -Since $m | Out-Null
Start-Sleep -Milliseconds 1200
Save-Screenshot 'edit_current' | Out-Null
Close-Screen
Close-Screen

Invoke-Chat '/waystones debug selection costs'
Invoke-Chat '/waystones debug selection clear'
