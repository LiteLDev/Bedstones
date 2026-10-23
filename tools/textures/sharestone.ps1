# Unit U2: the 15 dyed sharestones (docs/SPEC.md §1.3). Original pixel art + generated pack files:
#   assets/pack/rp/textures/blocks/sharestone/<color>_sharestone.png   48x32 UV sheet (stones_common.ps1)
#   assets/pack/rp/models/blocks/sharestone.geo.json                   lower / upper / item geometry
#   assets/pack/rp/{terrain_texture.d,blocks.d}/sharestone.json, texts.d/<locale>/sharestone.lang
#   assets/pack/bp/blocks/<color>_sharestone.json, recipes/<color>_sharestone.json
#
#   pwsh tools/textures/sharestone.ps1 [-Preview]   (-Preview also writes 8x previews to tools/textures/preview/)
param([switch]$Preview)

. "$PSScriptRoot/lib.ps1"
. "$PSScriptRoot/stones_common.ps1"

$root = (Resolve-Path "$PSScriptRoot/../..").Path
$rp = Join-Path $root 'assets/pack/rp'
$bp = Join-Path $root 'assets/pack/bp'

# ---- shape (Java: lower half like the waystone, upper half widening to 16x3 at the top) ---------------
$lower = @(
    (New-Box 0 0 0 16 3 16),
    (New-Box 1 3 1 14 4 14),
    (New-Box 2 7 2 12 2 12),
    (New-Box 3 9 3 10 7 10)
)
$upper = @(
    (New-Box 3 0 3 10 8 10),
    (New-Box 2 8 2 12 2 12),
    (New-Box 1 10 1 14 3 14),
    (New-Box 0 13 0 16 3 16),
    # The floating warp stone (Java renders a spinning item; here a static gem above the top).
    (New-Box 6 19 6 4 4 4 @{ rot = @(0, 45, 0); pivot = @(8, 21, 8); allUv = @(32, 0, 4, 4) })
)
Save-StoneGeometry (Join-Path $rp 'models/blocks/sharestone.geo.json') 'sharestone' $lower $upper

# ---- texture ---------------------------------------------------------------------------------------------
# Rune column on the pillar (rows 9..22 span the joint of both halves).
$runes = @(
    '.rr.r.',
    '.r..r.',
    '.rurr.',
    '....r.',
    'r.....',
    'rrr.rr',
    '..r..r',
    '.rur.r',
    '......',
    '.r..r.',
    'rrrru.',
    '.r..r.',
    '......',
    'r.rr.r'
)
function New-SharestoneSheet {
    $g = New-Sheet
    # Side picture, upper half (rows 0..15) then lower half (rows 16..31).
    Fill-Band $g 0 15 0 2 'trim'
    Fill-Band $g 0 15 3 5 'polished'
    Fill-Band $g 0 15 6 7 'dark'
    Fill-Band $g 0 15 8 22 'brick'
    Fill-Band $g 0 15 23 24 'dark'
    Fill-Band $g 0 15 25 28 'brick' 3
    Fill-Band $g 0 15 29 31 'polished'
    for ($x = 0; $x -lt 16; $x++) { $g[31][$x] = 'a' }
    Draw-Runes $g 5 9 $runes
    # Top faces: the cap with a rune ring in the middle.
    Fill-Band $g 16 31 0 15 'brick' 5
    for ($i = 16; $i -le 31; $i++) { $g[0][$i] = 'a'; $g[15][$i] = 'a' }
    for ($j = 0; $j -le 15; $j++) { $g[$j][16] = 'a'; $g[$j][31] = 'a' }
    for ($j = 0; $j -le 15; $j++) {
        for ($i = 0; $i -le 15; $i++) {
            $dx = $i - 7.5; $dy = $j - 7.5; $r = [Math]::Sqrt($dx * $dx + $dy * $dy)
            if ($r -ge 3.6 -and $r -lt 4.6) { $g[$j][16 + $i] = 'r' }
            elseif ($r -lt 1.6) { $g[$j][16 + $i] = 'u' }
        }
    }
    # Bottom faces and extras.
    Fill-Band $g 16 31 16 31 'dark' 7
    Fill-Band $g 32 47 0 31 'polished' 9
    Draw-Gem $g 32 0
    return , $g
}

$terrain = [ordered]@{}
$blocks = [ordered]@{}
$en = @(); $zh = @()
foreach ($color in $StoneColors) {
    if ($color -eq 'white') { continue } # there is no white sharestone
    $path = "$($color)_sharestone"
    $png = Join-Path $rp "textures/blocks/sharestone/$path.png"
    $sheet = New-SharestoneSheet
    Save-Sheet $sheet $color $png
    if ($Preview) {
        $rows = foreach ($line in $sheet) { -join $line }
        Save-PixelArt -Rows $rows -Palette (Get-StonePalette $color) -Path (Join-Path $PSScriptRoot "preview/$path.png") -Scale 8
    }
    $terrain["bs_$path"] = [ordered]@{ textures = "textures/blocks/sharestone/$path" }
    $blocks["bedstones:$path"] = [ordered]@{ sound = 'stone' }
    $en += "tile.bedstones:$path.name=$($ColorNames[$color]) Sharestone"
    $zh += "tile.bedstones:$path.name=$($ColorNamesZh[$color])共享石碑"

    $box = Get-Box @(-8, 0, -8) @(16, 16, 16)
    Save-Json (New-StoneBlock $path 'sharestone' "bs_$path" $box $box) (Join-Path $bp "blocks/$path.json")
    $key = [ordered]@{
        S = [ordered]@{ item = 'minecraft:stone_bricks' }
        D = [ordered]@{ item = "minecraft:$($color)_dye" }
        W = [ordered]@{ item = 'bedstones:warp_stone' }
        O = [ordered]@{ item = 'minecraft:obsidian' }
    }
    Save-Json (New-ShapedRecipe $path @('SSS', 'DWD', 'OOO') $key) (Join-Path $bp "recipes/$path.json")
}
Save-Json $terrain (Join-Path $rp 'terrain_texture.d/sharestone.json')
Save-Json $blocks (Join-Path $rp 'blocks.d/sharestone.json')
Save-Text (($en -join "`n") + "`n") (Join-Path $rp 'texts.d/en_US/sharestone.lang')
Save-Text (($zh -join "`n") + "`n") (Join-Path $rp 'texts.d/zh_CN/sharestone.lang')
Save-Json ([ordered]@{ pools = @() }) (Join-Path $bp 'loot_tables/blocks/bedstones_stones_none.json')
Write-Host "sharestone assets written"
