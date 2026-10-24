# Draws the textures and block models of unit U1 (the 10 waystone styles), original pixel art:
#   assets/pack/rp/textures/blocks/waystone/<style>.png   64x32 UV sheet per style
#   assets/pack/rp/models/blocks/waystone.geo.json        geometry.bedstones_waystone_{bottom,top,item}
# and the pack JSON of the styles (BP blocks / loot tables / recipes, RP terrain_texture + blocks
# fragments), so all ten stay consistent.
#
# UV sheet (x, y, w, h):
#   (0, 0, 16, 32)   side column: rows 0-15 = upper half, rows 16-31 = lower half (row 31 = y 0)
#   (16, 0, 16, 32)  front column (same rows, with the big rune), the model's north face
#   (32, 0, 16, 16)  top faces
#   (48, 0, 16, 16)  bottom faces
# Every cube maps its faces onto the slice of the column at its own height, so the tiers of the
# model show one continuous carved stone.
#
#   pwsh tools/textures/waystone.ps1 [-Preview]   (-Preview also writes 8x previews to tools/textures/preview/)
param([switch]$Preview)

. "$PSScriptRoot/lib.ps1"

$root = (Resolve-Path "$PSScriptRoot/../..").Path

# ---- shapes (docs/SPEC.md §1.2), in pixels: width (= depth), y from, y to --------------------------
$lowerCubes = @(
    @{ w = 16; y0 = 0; y1 = 3 },
    @{ w = 14; y0 = 3; y1 = 7 },
    @{ w = 12; y0 = 7; y1 = 9 },
    @{ w = 10; y0 = 9; y1 = 16 }
)
$upperCubes = @(
    @{ w = 10; y0 = 0; y1 = 8 },
    @{ w = 12; y0 = 8; y1 = 10 },
    @{ w = 14; y0 = 10; y1 = 12 },
    @{ w = 10; y0 = 12; y1 = 14 },
    @{ w = 8; y0 = 14; y1 = 16 }
)

# ---- styles: base stone colour, accent and rune colour (white unless the spec says otherwise) -----
$styles = [ordered]@{
    'waystone'                   = @{ base = '#8a8a86'; accent = '#6f6f6b'; rune = '#ffffff'; kind = 'bricks' }
    'mossy_waystone'             = @{ base = '#828680'; accent = '#5b7a2e'; rune = '#ffffff'; kind = 'mossy' }
    'sandy_waystone'             = @{ base = '#d9ca97'; accent = '#c2ad74'; rune = '#ffffff'; kind = 'chiseled' }
    'deepslate_waystone'         = @{ base = '#4f4f55'; accent = '#3a3a40'; rune = '#ffffff'; kind = 'streaks' }
    'blackstone_waystone'        = @{ base = '#3a3238'; accent = '#2a2329'; rune = '#993333'; kind = 'bricks' }
    'end_stone_waystone'         = @{ base = '#dedfa6'; accent = '#c7c78c'; rune = '#7200ff'; kind = 'bricks' }
    'red_nether_bricks_waystone' = @{ base = '#5a0f12'; accent = '#420a0c'; rune = '#ffffff'; kind = 'bricks' }
    'purpur_waystone'            = @{ base = '#a77ca7'; accent = '#8c638c'; rune = '#ffffff'; kind = 'tiles' }
    'prismarine_waystone'        = @{ base = '#64a596'; accent = '#4d8b80'; rune = '#ffffff'; kind = 'mottled' }
    'mud_bricks_waystone'        = @{ base = '#8b6b4f'; accent = '#735740'; rune = '#ffffff'; kind = 'bricks' }
}

# Rows of the column (0..31) where a tier starts (its top row) and ends (its bottom row).
function Get-TierRows {
    $tiers = @()
    foreach ($c in $upperCubes) { $tiers += , @((15 - ($c.y1 - 1)), (15 - $c.y0)) }
    foreach ($c in $lowerCubes) { $tiers += , @((31 - ($c.y1 - 1)), (31 - $c.y0)) }
    return , $tiers
}

function New-Grid([int]$w, [int]$h, [char]$c) {
    $g = New-Object 'char[][]' $h
    for ($y = 0; $y -lt $h; $y++) { $g[$y] = ([string]$c * $w).ToCharArray() }
    return , $g
}

# A 6x11 rune glyph (front) and a 4x7 one (sides); '#' = rune.
$bigRune = @(
    '..##..',
    '.#..#.',
    '#....#',
    '.#..#.',
    '..##..',
    '..##..',
    '#.##.#',
    '.####.',
    '..##..',
    '.#..#.',
    '#....#'
)
$smallRune = @(
    '.##.',
    '#..#',
    '.##.',
    '.##.',
    '####',
    '.##.',
    '#..#'
)

function Stamp($g, [string[]]$glyph, [int]$x, [int]$y, [char]$c) {
    for ($j = 0; $j -lt $glyph.Count; $j++) {
        for ($i = 0; $i -lt $glyph[$j].Length; $i++) {
            if ($glyph[$j][$i] -eq '#') { $g[$y + $j][$x + $i] = $c }
        }
    }
}

function Draw-Style([string]$name, $s) {
    $rng = [System.Random]::new(($name.GetHashCode() -band 0x7fffffff))
    $g = New-Grid 64 32 'a'
    # Stone noise everywhere.
    for ($y = 0; $y -lt 32; $y++) {
        for ($x = 0; $x -lt 64; $x++) {
            $r = $rng.NextDouble()
            if ($r -lt 0.18) { $g[$y][$x] = 'b' } elseif ($r -lt 0.34) { $g[$y][$x] = 'c' }
        }
    }
    # Material pattern on the columns (x 0..31).
    switch ($s.kind) {
        'bricks' {
            for ($y = 0; $y -lt 32; $y++) {
                $mortarRow = ($y % 4) -eq 3
                for ($x = 0; $x -lt 32; $x++) {
                    $off = if ((([int][Math]::Floor($y / 4)) % 2) -eq 0) { 0 } else { 4 }
                    if ($mortarRow -or (($x + $off) % 8) -eq 7) { $g[$y][$x] = 'd' }
                }
            }
        }
        'tiles' {
            for ($y = 0; $y -lt 32; $y++) { for ($x = 0; $x -lt 32; $x++) { if (($y % 8) -eq 7 -or ($x % 8) -eq 7) { $g[$y][$x] = 'd' } } }
        }
        'chiseled' {
            for ($y = 0; $y -lt 32; $y++) { for ($x = 0; $x -lt 32; $x++) { if (($y % 5) -eq 4) { $g[$y][$x] = 'd' } elseif (($y % 5) -eq 0) { $g[$y][$x] = 'b' } } }
        }
        'streaks' {
            for ($y = 0; $y -lt 32; $y += 3) { for ($x = 0; $x -lt 32; $x++) { if ($rng.NextDouble() -lt 0.6) { $g[$y][$x] = 'd' } } }
        }
        'mottled' {
            for ($k = 0; $k -lt 60; $k++) {
                $x = $rng.Next(0, 31); $y = $rng.Next(0, 31)
                $g[$y][$x] = 'x'; $g[$y][$x + 1] = 'x'; $g[$y + 1][$x] = 'x'
            }
        }
        'mossy' {
            for ($y = 0; $y -lt 32; $y++) {
                for ($x = 0; $x -lt 32; $x++) { if (($y % 4) -eq 3 -or (($x + (([int][Math]::Floor($y / 4)) % 2) * 4) % 8) -eq 7) { $g[$y][$x] = 'd' } }
            }
            # Moss creeps up from the base and hangs from the cap.
            for ($x = 0; $x -lt 64; $x++) {
                $hBottom = $rng.Next(2, 9); $hTop = $rng.Next(0, 4)
                for ($j = 0; $j -lt $hBottom; $j++) { if ($x -lt 32) { $g[31 - $j][$x] = if ($rng.NextDouble() -lt 0.3) { 'n' } else { 'm' } } }
                for ($j = 0; $j -lt $hTop; $j++) { if ($x -lt 32) { $g[$j][$x] = if ($rng.NextDouble() -lt 0.3) { 'n' } else { 'm' } } }
            }
            for ($y = 0; $y -lt 16; $y++) { for ($x = 32; $x -lt 48; $x++) { if ($rng.NextDouble() -lt 0.45) { $g[$y][$x] = 'm' } } }
        }
    }
    # Tier bevels: highlight on a tier's top row, shadow on its bottom row.
    foreach ($t in (Get-TierRows)) {
        for ($x = 0; $x -lt 32; $x++) {
            if ($g[$t[0]][$x] -notin 'm', 'n') { $g[$t[0]][$x] = 'e' }
            if ($t[1] - $t[0] -ge 2 -and $g[$t[1]][$x] -notin 'm', 'n') { $g[$t[1]][$x] = 'f' }
        }
    }
    # The shaft (rows 8..22, 10 px wide = x 3..12 of a column) gets a smooth face for the runes.
    for ($y = 8; $y -le 22; $y++) {
        foreach ($cx in 0, 16) {
            for ($x = $cx + 4; $x -le $cx + 11; $x++) { if ($g[$y][$x] -eq 'd') { $g[$y][$x] = 'a' } }
        }
    }
    # Runes are only engraved ('s') into the stone: the coloured, glowing runes are drawn by the client
    # mod's visuals for the players who activated the waystone (as Waystones' runes renderer does).
    Stamp $g $bigRune 22 11 's'
    Stamp $g $bigRune 21 10 's'
    Stamp $g $smallRune 6 9 's'
    Stamp $g $smallRune 6 16 's'
    # Top: a carved ring; bottom: plain darker stone.
    for ($y = 0; $y -lt 16; $y++) {
        for ($x = 32; $x -lt 48; $x++) {
            $dx = $x - 39.5; $dy = $y - 7.5; $d = [Math]::Sqrt($dx * $dx + $dy * $dy)
            if ($d -gt 2.5 -and $d -lt 3.6) { $g[$y][$x] = 's' } elseif ($d -le 1.2) { $g[$y][$x] = 's' }
        }
        for ($x = 48; $x -lt 64; $x++) { if ($g[$y][$x] -eq 'b') { $g[$y][$x] = 'c' } }
    }
    $base = $s.base
    $pal = @{
        a = $base
        b = (Shade-Color $base 1.12)
        c = (Shade-Color $base 0.9)
        d = (Shade-Color $s.accent 0.8)
        e = (Shade-Color $base 1.22)
        f = (Shade-Color $base 0.68)
        r = (Mix-Color (Shade-Color $base 0.6) $s.rune 0.7)
        s = (Shade-Color $base 0.5)
        x = $s.accent
        m = '#5d7f2c'
        n = '#46651f'
    }
    $rows = foreach ($row in $g) { -join $row }
    $path = Join-Path $root "assets/pack/rp/textures/blocks/waystone/$name.png"
    Save-PixelArt -Rows $rows -Palette $pal -Path $path
    if ($Preview) { Save-PixelArt -Rows $rows -Palette $pal -Path (Join-Path $PSScriptRoot "preview/waystone_$name.png") -Scale 8 }
}

foreach ($name in $styles.Keys) { Draw-Style $name $styles[$name] }

# ---- geometry -------------------------------------------------------------------------------------
# rowBase: 0 for the upper half's column rows, 16 for the lower half's. `scale` / `yOffset` build
# the item model (the whole waystone at half size).
function New-Cube($c, [int]$rowBase, [double]$scale = 1, [double]$yOffset = 0) {
    $w = $c.w; $h = $c.y1 - $c.y0
    $u = 8 - $w / 2
    $v = $rowBase + (16 - $c.y1)
    $side = @{ uv = @($u, $v); uv_size = @($w, $h) }
    return [ordered]@{
        origin = @(((-$w / 2) * $scale), (($c.y0 * $scale) + $yOffset), ((-$w / 2) * $scale))
        size   = @(($w * $scale), ($h * $scale), ($w * $scale))
        uv     = [ordered]@{
            north = @{ uv = @((16 + $u), $v); uv_size = @($w, $h) }
            south = $side
            east  = $side
            west  = $side
            up    = @{ uv = @((32 + $u), $u); uv_size = @($w, $w) }
            down  = @{ uv = @((48 + $u), $u); uv_size = @($w, $w) }
        }
    }
}
function New-Geometry([string]$id, $cubes) {
    return [ordered]@{
        description = [ordered]@{
            identifier = $id; texture_width = 64; texture_height = 32
            visible_bounds_width = 2; visible_bounds_height = 3; visible_bounds_offset = @(0, 1, 0)
        }
        bones = @([ordered]@{ name = 'waystone'; pivot = @(0, 0, 0); cubes = $cubes })
    }
}
$bottom = @($lowerCubes | ForEach-Object { New-Cube $_ 16 })
$top = @($upperCubes | ForEach-Object { New-Cube $_ 0 })
$item = @($lowerCubes | ForEach-Object { New-Cube $_ 16 0.5 0 }) + @($upperCubes | ForEach-Object { New-Cube $_ 0 0.5 8 })
$geo = [ordered]@{
    format_version      = '1.16.0'
    'minecraft:geometry' = @(
        (New-Geometry 'geometry.bedstones_waystone_bottom' $bottom),
        (New-Geometry 'geometry.bedstones_waystone_top' $top),
        (New-Geometry 'geometry.bedstones_waystone_item' $item)
    )
}
$geoPath = Join-Path $root 'assets/pack/rp/models/blocks/waystone.geo.json'
New-Item -ItemType Directory -Force (Split-Path -Parent $geoPath) | Out-Null
($geo | ConvertTo-Json -Depth 12) | Set-Content -Path $geoPath -Encoding utf8NoBOM

# ---- pack JSON (kept in sync with the styles above) ---------------------------------------------------
# BP blocks/<style>.json, loot_tables/blocks/<style>.json, recipes/<style>*.json; RP fragments
# terrain_texture.d/waystone.json, blocks.d/waystone.json.
$materials = @{
    'waystone' = 'minecraft:stone_bricks'; 'mossy_waystone' = 'minecraft:mossy_stone_bricks'
    'sandy_waystone' = 'minecraft:chiseled_sandstone'; 'deepslate_waystone' = 'minecraft:deepslate'
    'blackstone_waystone' = 'minecraft:blackstone'; 'end_stone_waystone' = 'minecraft:end_bricks'
    'red_nether_bricks_waystone' = 'minecraft:red_nether_brick'; 'purpur_waystone' = 'minecraft:purpur_block'
    'prismarine_waystone' = 'minecraft:prismarine'; 'mud_bricks_waystone' = 'minecraft:mud_bricks'
}
# Bedrock block sound per style (Java: STONE, DEEPSLATE, NETHER_BRICKS, MUD_BRICKS).
$sounds = @{ 'deepslate_waystone' = 'deepslate'; 'red_nether_bricks_waystone' = 'nether_brick'; 'mud_bricks_waystone' = 'mud_bricks' }

function Write-Json($obj, [string]$rel) {
    $p = Join-Path $root $rel
    New-Item -ItemType Directory -Force (Split-Path -Parent $p) | Out-Null
    ($obj | ConvertTo-Json -Depth 20) | Set-Content -Path $p -Encoding utf8NoBOM
}

$terrain = [ordered]@{}
$blocksRp = [ordered]@{}
foreach ($name in $styles.Keys) {
    $id = "bedstones:$name"
    $tex = "bedstones_$name"
    $terrain[$tex] = @{ textures = "textures/blocks/waystone/$name" }
    $blocksRp[$id] = @{ sound = $(if ($sounds.Contains($name)) { $sounds[$name] } else { 'stone' }) }
    $material = @{ '*' = [ordered]@{ texture = $tex; render_method = 'opaque' } }
    $permutations = @(
        @{ condition = "q.block_state('bedstones:facing') == 0"; components = @{ 'minecraft:transformation' = @{ rotation = @(0, 180, 0) } } },
        @{ condition = "q.block_state('bedstones:facing') == 1"; components = @{ 'minecraft:transformation' = @{ rotation = @(0, 90, 0) } } },
        @{ condition = "q.block_state('bedstones:facing') == 2"; components = @{ 'minecraft:transformation' = @{ rotation = @(0, 0, 0) } } },
        @{ condition = "q.block_state('bedstones:facing') == 3"; components = @{ 'minecraft:transformation' = @{ rotation = @(0, 270, 0) } } },
        @{
            condition  = "q.block_state('bedstones:upper') == 1"
            components = [ordered]@{
                'minecraft:geometry'      = 'geometry.bedstones_waystone_top'
                'minecraft:collision_box' = @{ origin = @(-6, 0, -6); size = @(12, 16, 12) }
                'minecraft:selection_box' = @{ origin = @(-7, 0, -7); size = @(14, 16, 14) }
            }
        }
    )
    $block = [ordered]@{
        format_version   = '1.26.0'
        'minecraft:block' = [ordered]@{
            description  = [ordered]@{
                identifier    = $id
                menu_category = @{ category = 'construction' }
                states        = [ordered]@{ 'bedstones:facing' = @(0, 1, 2, 3); 'bedstones:upper' = @(0, 1) }
            }
            components   = [ordered]@{
                'minecraft:display_name'              = "tile.$id.name"
                'minecraft:geometry'                  = 'geometry.bedstones_waystone_bottom'
                'minecraft:material_instances'        = $material
                'minecraft:item_visual'               = [ordered]@{ geometry = 'geometry.bedstones_waystone_item'; material_instances = $material }
                'minecraft:collision_box'             = @{ origin = @(-7, 0, -7); size = @(14, 16, 14) }
                'minecraft:selection_box'             = @{ origin = @(-8, 0, -8); size = @(16, 16, 16) }
                'minecraft:destructible_by_mining'    = @{ seconds_to_destroy = 5 }
                # Pickaxes mine it at their tier speed (Java: hardness 5, pickaxe tool).
                'tag:stone'                           = @{}
                'tag:minecraft:is_pickaxe_item_destructible' = @{}
                'minecraft:destructible_by_explosion' = @{ explosion_resistance = 2000 }
                'minecraft:movable'                   = @{ movement_type = 'immovable' }
                'minecraft:loot'                      = "loot_tables/blocks/$name.json"
                'minecraft:light_dampening'           = 0
                'minecraft:map_color'                 = $styles[$name].base
            }
            permutations = $permutations
        }
    }
    Write-Json $block "assets/pack/bp/blocks/$name.json"
    # Drops are made in C++ (lower half only, silk touch keeps the waystone uid).
    Write-Json @{ pools = @() } "assets/pack/bp/loot_tables/blocks/$name.json"
    Write-Json ([ordered]@{
            format_version          = '1.20.10'
            'minecraft:recipe_shaped' = [ordered]@{
                description = @{ identifier = $id }
                tags        = @('crafting_table')
                pattern     = @(' S ', 'SWS', 'OOO')
                key         = [ordered]@{
                    S = @{ item = $materials[$name] }
                    W = @{ item = 'bedstones:warp_stone' }
                    O = @{ item = 'minecraft:obsidian' }
                }
                unlock      = @(@{ item = 'bedstones:warp_stone' })
                result      = @{ item = $id }
            }
        }) "assets/pack/bp/recipes/$name.json"
}
foreach ($extra in @(@{ suffix = 'from_moss_block'; item = 'minecraft:moss_block' }, @{ suffix = 'from_vine'; item = 'minecraft:vine' })) {
    Write-Json ([ordered]@{
            format_version             = '1.20.10'
            'minecraft:recipe_shapeless' = [ordered]@{
                description = @{ identifier = "bedstones:mossy_waystone_$($extra.suffix)" }
                tags        = @('crafting_table')
                ingredients = @(@{ item = 'bedstones:waystone' }, @{ item = $extra.item; count = 3 })
                unlock      = @(@{ item = 'bedstones:waystone' })
                result      = @{ item = 'bedstones:mossy_waystone' }
            }
        }) "assets/pack/bp/recipes/mossy_waystone_$($extra.suffix).json"
}
Write-Json $terrain 'assets/pack/rp/terrain_texture.d/waystone.json'
Write-Json $blocksRp 'assets/pack/rp/blocks.d/waystone.json'
Write-Host "waystone textures, models and pack JSON written"
