# Draws the textures of unit U3 (warp plate + shards), original pixel art, and writes the plate's
# block JSON (its 32 status x colour permutations are generated here so they stay in sync):
#   assets/pack/rp/textures/blocks/warp_plate_base.png         stone-brick base (16x16)
#   assets/pack/rp/textures/blocks/warp_plate_inner.png        empty centre (no shard)
#   assets/pack/rp/textures/blocks/warp_plate_lock.png         centre with a lock (deepslate shard)
#   assets/pack/rp/textures/blocks/warp_plate_runes_<i>.png    centre runes, galactic colour i (0..14)
#   assets/pack/rp/textures/blocks/warp_plate_runes_glow_<i>.png  brighter runes while warping
#   assets/pack/rp/textures/items/{warp_dust,dormant_shard,attuned_shard,crumbling_attuned_shard,deepslate_shard}.png
#   assets/pack/bp/blocks/warp_plate.json
#
#   pwsh tools/textures/warpplate.ps1 [-Preview]   (-Preview also writes 8x previews to tools/textures/preview/)
param([switch]$Preview)

. "$PSScriptRoot/lib.ps1"

$root   = (Resolve-Path "$PSScriptRoot/../..").Path
$blocks = Join-Path $root 'assets/pack/rp/textures/blocks'
$items  = Join-Path $root 'assets/pack/rp/textures/items'

function Save([string[]]$rows, [hashtable]$pal, [string]$path) {
    Save-PixelArt -Rows $rows -Palette $pal -Path $path
    if ($Preview) {
        $name = [IO.Path]::GetFileNameWithoutExtension($path)
        Save-PixelArt -Rows $rows -Palette $pal -Path (Join-Path $PSScriptRoot "preview/$name.png") -Scale 8
    }
}

# ---- base: worn stone bricks, a darker rim ------------------------------------------------------
$base = @(
    'oooooooooooooooo',
    'oHHHHHHdHHHHHHHo',
    'oHsssssdHssssssn',
    'osssssldssslsssn',
    'odddddddddddddno',
    'oHHHdHHHHHHHdHHo',
    'osssdsssslsssdsn',
    'oslsdsssssssdssn',
    'oddddddddddddddo',
    'oHHHHHHHdHHHHHHo',
    'osslsssddssssssn',
    'osssssssdssslssn',
    'odddddddddddddno',
    'oHHHdHHHHHHHHdHo',
    'osssdssslssssdsn',
    'onnnnnnnnnnnnnno'
)
$basePal = @{ o = '#4a4a4f'; H = '#a2a2a6'; s = '#8a8a8f'; l = '#97979c'; d = '#5d5d63'; n = '#3c3c41' }
Save $base $basePal (Join-Path $blocks 'warp_plate_base.png')

# ---- centre panels (the 10x10 centre at 3,3 is what the model shows) ---------------------------
function Centre([string[]]$inner) {
    $rows = @()
    for ($y = 0; $y -lt 16; $y++) {
        if ($y -lt 3 -or $y -ge 13) { $rows += 'kkkkkkkkkkkkkkkk' }
        else { $rows += 'kkk' + $inner[$y - 3] + 'kkk' }
    }
    return , $rows
}
$empty = Centre @(
    'bbbbbbbbbb',
    'bkkkkkkkkb',
    'bkmkkkkmkb',
    'bkkkmkkkkb',
    'bkkkkkmkkb',
    'bkkmkkkkkb',
    'bkkkkmkkkb',
    'bkmkkkkmkb',
    'bkkkkkkkkb',
    'bbbbbbbbbb'
)
$runes = Centre @(
    'bbbbbbbbbb',
    'bmrrkkrrmb',
    'brkkrrkkrb',
    'brkqkkqkrb',
    'bkrkqqkrkb',
    'bkrkqqkrkb',
    'brkqkkqkrb',
    'brkkrrkkrb',
    'bmrrkkrrmb',
    'bbbbbbbbbb'
)
$lock = Centre @(
    'bbbbbbbbbb',
    'bkkkkkkkkb',
    'bkkkggkkkb',
    'bkkgkkgkkb',
    'bkkgkkgkkb',
    'bkyyyyyykb',
    'bkyyddyykb',
    'bkyyddyykb',
    'bkyyyyyykb',
    'bbbbbbbbbb'
)
$panel = @{ k = '#1d1626'; m = '#2c2238'; b = '#5d5d63' }
Save $empty $panel (Join-Path $blocks 'warp_plate_inner.png')
Save $lock ($panel + @{ g = '#9a9aa2'; y = '#6c6c75'; d = '#2a2a30' }) (Join-Path $blocks 'warp_plate_lock.png')

# Galactic colours: ChatFormatting colour of index i after Waystones' remapping
# (gray -> light purple, dark gray -> dark purple, black -> gold).
$fmt   = @('#000000', '#0000AA', '#00AA00', '#00AAAA', '#AA0000', '#AA00AA', '#FFAA00', '#AAAAAA',
           '#555555', '#5555FF', '#55FF55', '#55FFFF', '#FF5555', '#FF55FF', '#FFFF55', '#FFFFFF')
$remap = @{ 7 = 13; 8 = 5; 0 = 6 }
for ($i = 0; $i -lt 15; $i++) {
    $id = if ($remap.ContainsKey($i)) { $remap[$i] } else { $i }
    $c  = $fmt[$id]
    Save $runes ($panel + @{ r = (Shade-Color $c 0.85); q = (Shade-Color $c 1.25) }) (Join-Path $blocks "warp_plate_runes_$i.png")
    Save $runes (@{ k = '#2d2140'; m = '#45335c'; b = '#6a6a72'; r = (Shade-Color $c 1.35); q = (Shade-Color $c 1.7) }) (Join-Path $blocks "warp_plate_runes_glow_$i.png")
}

# ---- items -------------------------------------------------------------------------------------------
$dust = @(
    '................',
    '................',
    '................',
    '.......w........',
    '..........p.....',
    '....p...........',
    '.......pq.......',
    '.....pqqqqp..w..',
    '....pqqwqqqp....',
    '...pqqqqqqwqp...',
    '..pqqwqqqqqqqp..',
    '..pdqqqqqwqqdp..',
    '...ddqqqqqqdd...',
    '....dddddddd....',
    '................',
    '................'
)
Save $dust @{ p = '#7a3fb5'; q = '#a45ee0'; w = '#e7c8ff'; d = '#4e2478' } (Join-Path $items 'warp_dust.png')

$shard = @(
    '................',
    '..........o.....',
    '.........oLo....',
    '........oLMo....',
    '.......oLMMo....',
    '......oLMMDo....',
    '.....oLMMMDo....',
    '.....oLMMDo.....',
    '....oLMMMDo.....',
    '....oLMMDo......',
    '...oLMMMDo......',
    '...oLMMDo.......',
    '..oLMDDo........',
    '..oMDDo.........',
    '...ooo..........',
    '................'
)
Save $shard @{ o = '#2b2433'; L = '#b9b2c2'; M = '#8a8296'; D = '#5f586b' } (Join-Path $items 'dormant_shard.png')
Save $shard @{ o = '#3b1460'; L = '#f2c9ff'; M = '#c46cf0'; D = '#8a2fc0' } (Join-Path $items 'attuned_shard.png')
Save $shard @{ o = '#262629'; L = '#77777d'; M = '#4b4b52'; D = '#333338' } (Join-Path $items 'deepslate_shard.png')
$crumbling = Merge-Rows $shard @(
    '................',
    '................',
    '................',
    '..........x.....',
    '................',
    '........x.......',
    '.......xx.......',
    '................',
    '.........x......',
    '......xx........',
    '................',
    '.....x.....c....',
    '................',
    '..........c.....',
    '.c..............',
    '.........c......'
)
Save $crumbling @{ o = '#3b1460'; L = '#e0b6f2'; M = '#a95ad6'; D = '#7a2aa8'; x = '#3b1460'; c = '#7a2aa8' } (Join-Path $items 'crumbling_attuned_shard.png')

# ---- block JSON ---------------------------------------------------------------------------------------
function Inner([string]$tex) {
    return [ordered]@{
        '*'     = [ordered]@{ texture = 'bedstones_warp_plate_base'; render_method = 'alpha_test' }
        'inner' = [ordered]@{ texture = $tex; render_method = 'alpha_test' }
    }
}
$perms = @(
    [ordered]@{ condition = "q.block_state('bedstones:status') == 0"; components = [ordered]@{ 'minecraft:material_instances' = (Inner 'bedstones_warp_plate_inner') } },
    [ordered]@{ condition = "q.block_state('bedstones:status') == 6"; components = [ordered]@{ 'minecraft:material_instances' = (Inner 'bedstones_warp_plate_lock') } }
)
for ($i = 0; $i -lt 15; $i++) {
    $perms += [ordered]@{
        condition  = "q.block_state('bedstones:status') != 0 && q.block_state('bedstones:status') != 6 && q.block_state('bedstones:status') != 3 && q.block_state('bedstones:plate_color') == $i"
        components = [ordered]@{ 'minecraft:material_instances' = (Inner "bedstones_warp_plate_runes_$i") }
    }
    $perms += [ordered]@{
        condition  = "q.block_state('bedstones:status') == 3 && q.block_state('bedstones:plate_color') == $i"
        components = [ordered]@{ 'minecraft:material_instances' = (Inner "bedstones_warp_plate_runes_glow_$i"); 'minecraft:light_emission' = 7 }
    }
}
$block = [ordered]@{
    format_version    = '1.26.0'
    'minecraft:block' = [ordered]@{
        description  = [ordered]@{
            identifier    = 'bedstones:warp_plate'
            menu_category = [ordered]@{ category = 'construction' }
            states        = [ordered]@{
                'bedstones:status'      = @(0, 1, 2, 3, 4, 5, 6)
                'bedstones:plate_color' = @(0..14)
            }
        }
        components   = [ordered]@{
            'minecraft:display_name'                       = 'tile.bedstones:warp_plate.name'
            'minecraft:geometry'                           = 'geometry.bedstones_warp_plate'
            'minecraft:material_instances'                 = (Inner 'bedstones_warp_plate_inner')
            'minecraft:collision_box'                      = [ordered]@{ origin = @(-8, 0, -8); size = @(16, 1, 16) }
            'minecraft:selection_box'                      = [ordered]@{ origin = @(-8, 0, -8); size = @(16, 2, 16) }
            'minecraft:destructible_by_mining'             = [ordered]@{ seconds_to_destroy = 5 }
            'minecraft:destructible_by_explosion'          = [ordered]@{ explosion_resistance = 2000 }
            'minecraft:movable'                            = [ordered]@{ movement_type = 'immovable' }
            'minecraft:loot'                               = 'loot_tables/blocks/bedstones_warp_plate.json'
            'minecraft:light_dampening'                    = 0
            'minecraft:map_color'                          = '#8a8a8f'
            'tag:minecraft:is_pickaxe_item_destructible'   = [ordered]@{}
            'tag:stone'                                    = [ordered]@{}
        }
        permutations = $perms
    }
}
$json = $block | ConvertTo-Json -Depth 20
$out  = Join-Path $root 'assets/pack/bp/blocks/warp_plate.json'
New-Item -ItemType Directory -Force (Split-Path -Parent $out) | Out-Null
Set-Content -Path $out -Value $json -Encoding utf8NoBOM
Write-Host 'warp plate textures and block JSON written'
