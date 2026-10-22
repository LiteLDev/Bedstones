# Unit U2: the 16 portstones (docs/SPEC.md §1.4). Original pixel art + generated pack files:
#   assets/pack/rp/textures/blocks/portstone/<color>_portstone.png   48x32 UV sheet (stones_common.ps1)
#   assets/pack/rp/models/blocks/portstone.geo.json                  lower / upper / item geometry
#   assets/pack/rp/{terrain_texture.d,blocks.d}/portstone.json, texts.d/<locale>/portstone.lang
#   assets/pack/bp/blocks/<color>_portstone.json, recipes/<color>_portstone.json
#
#   pwsh tools/textures/portstone.ps1 [-Preview]   (run sharestone.ps1 too: it writes the shared empty loot table)
param([switch]$Preview)

. "$PSScriptRoot/lib.ps1"
. "$PSScriptRoot/stones_common.ps1"

$root = (Resolve-Path "$PSScriptRoot/../..").Path
$rp = Join-Path $root 'assets/pack/rp'
$bp = Join-Path $root 'assets/pack/bp'

# ---- shape: a pedestal with a lectern-like slanted top (front = north, the low edge) ------------------
$plain = @{ faceUv = @{ up = @(16, 16, 14, 14) } } # hidden / plain tops sample the bottom area
$lower = @(
    (New-Box 1 0 1 14 2 14 $plain),
    (New-Box 2 2 2 12 2 12 $plain),
    (New-Box 4 4 4 8 12 8 $plain)
)
$upper = @(
    (New-Box 4 0 4 8 7 8 $plain),
    (New-Box 1 7 2 14 3 12 @{ rot = @(22.5, 0, 0); pivot = @(8, 8.5, 8); faceUv = @{ up = @(17, 2, 14, 12) } })
)
Save-StoneGeometry (Join-Path $rp 'models/blocks/portstone.geo.json') 'portstone' $lower $upper

# ---- texture ---------------------------------------------------------------------------------------------
$pillarRunes = @(
    '.rr.',
    '.r..',
    '.ru.',
    '..r.',
    '....',
    'r..r',
    'rurr',
    '....',
    '.r..',
    'rrr.',
    '.r..',
    '....',
    '..rr',
    '.ur.',
    'rr..',
    '....',
    '.rr.'
)
# The reading surface: a portal-like rune circle.
$topRunes = @(
    '...rrrr...',
    '..r....r..',
    '.r..uu..r.',
    'r..u..u..r',
    'r.u.rr.u.r',
    'r.u.rr.u.r',
    'r..u..u..r',
    '.r..uu..r.',
    '..r....r..'
)
function New-PortstoneSheet {
    $g = New-Sheet
    # Side picture: upper rows 0..15 (board sides at rows 6..8, pillar rows 9..15), lower rows 16..31.
    Fill-Band $g 0 15 0 5 'polished'
    Fill-Band $g 0 15 6 8 'trim'
    Fill-Band $g 0 15 9 27 'brick'
    Fill-Band $g 0 15 28 29 'polished'
    Fill-Band $g 0 15 30 31 'dark'
    Draw-Runes $g 6 10 $pillarRunes
    # Top area: the slanted board (17, 2, 14x12) with a frame and the rune circle.
    Fill-Band $g 16 31 0 15 'polished' 4
    for ($i = 17; $i -le 30; $i++) { $g[2][$i] = 'a'; $g[13][$i] = 'a' }
    for ($j = 2; $j -le 13; $j++) { $g[$j][17] = 'a'; $g[$j][30] = 'a' }
    for ($j = 3; $j -le 12; $j++) { for ($i = 18; $i -le 29; $i++) { $g[$j][$i] = if ((Get-Noise $i $j 11) -lt 20) { 'b' } else { 'c' } } }
    Draw-Runes $g 19 4 $topRunes
    # Bottom / plain tops and extras.
    Fill-Band $g 16 31 16 31 'polished' 7
    Fill-Band $g 32 47 0 31 'polished' 9
    return , $g
}

$terrain = [ordered]@{}
$blocks = [ordered]@{}
$en = @(); $zh = @()
foreach ($color in $StoneColors) {
    $path = "$($color)_portstone"
    $png = Join-Path $rp "textures/blocks/portstone/$path.png"
    $sheet = New-PortstoneSheet
    Save-Sheet $sheet $color $png
    if ($Preview) {
        $rows = foreach ($line in $sheet) { -join $line }
        Save-PixelArt -Rows $rows -Palette (Get-StonePalette $color) -Path (Join-Path $PSScriptRoot "preview/$path.png") -Scale 8
    }
    $terrain["bs_$path"] = [ordered]@{ textures = "textures/blocks/portstone/$path" }
    $blocks["bedstones:$path"] = [ordered]@{ sound = 'stone' }
    if ($color -eq 'white') {
        $en += "tile.bedstones:$path.name=Portstone"
        $zh += "tile.bedstones:$path.name=门户石碑"
    } else {
        $en += "tile.bedstones:$path.name=$($ColorNames[$color]) Portstone"
        $zh += "tile.bedstones:$path.name=$($ColorNamesZh[$color])门户石碑"
    }
    $lowerBox = Get-Box @(-7, 0, -7) @(14, 16, 14)
    $upperBox = Get-Box @(-7, 0, -7) @(14, 13, 14)
    Save-Json (New-StoneBlock $path 'portstone' "bs_$path" $lowerBox $upperBox) (Join-Path $bp "blocks/$path.json")
    $key = [ordered]@{
        D = [ordered]@{ item = "minecraft:$($color)_dye" }
        S = [ordered]@{ item = 'minecraft:stone_bricks' }
        W = [ordered]@{ item = 'bedstones:warp_stone' }
        B = [ordered]@{ item = 'minecraft:polished_andesite' }
    }
    Save-Json (New-ShapedRecipe $path @('DSD', 'SWS', 'BBB') $key) (Join-Path $bp "recipes/$path.json")
}
Save-Json $terrain (Join-Path $rp 'terrain_texture.d/portstone.json')
Save-Json $blocks (Join-Path $rp 'blocks.d/portstone.json')
Save-Text (($en -join "`n") + "`n") (Join-Path $rp 'texts.d/en_US/portstone.lang')
Save-Text (($zh -join "`n") + "`n") (Join-Path $rp 'texts.d/zh_CN/portstone.lang')
Write-Host "portstone assets written"
