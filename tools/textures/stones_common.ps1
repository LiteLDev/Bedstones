# Shared helpers of tools/textures/sharestone.ps1 and portstone.ps1 (unit U2): a 48x32 UV sheet
# per dyed block and the two-tall geometry / block JSON that maps onto it.
#
# UV sheet layout (48x32):
#   x  0..15  side faces of both halves as one 16x32 picture: rows 0..15 = upper half (y 16..0),
#             rows 16..31 = lower half. A side face of a box spanning y0..y1 of its half samples
#             rows (16 - y1)..(16 - y0) (+16 for the lower half), so bands line up with the shape.
#   x 16..31  rows 0..15: top faces (u = x, v = z of the box); rows 16..31: bottom faces.
#   x 32..47  extras (warp stone gem at 32,0 4x4).
#
# Dot-source after lib.ps1:  . "$PSScriptRoot/stones_common.ps1"

$script:StoneColors = @(
    'white', 'orange', 'magenta', 'light_blue', 'yellow', 'lime', 'pink', 'gray',
    'light_gray', 'cyan', 'purple', 'blue', 'brown', 'green', 'red', 'black'
)
# Java DyeColor diffuse colors (core::kDyeDiffuse).
$script:DyeDiffuse = @{
    white = '#F9FFFE'; orange = '#F9801D'; magenta = '#C74EBD'; light_blue = '#3AB3DA'; yellow = '#FED83D'
    lime = '#80C71F'; pink = '#F38BAA'; gray = '#474F52'; light_gray = '#9D9D97'; cyan = '#169C9C'
    purple = '#8932B8'; blue = '#3C44AA'; brown = '#835432'; green = '#5E7C16'; red = '#B02E26'; black = '#1D1D21'
}
$script:ColorNames = @{
    white = 'White'; orange = 'Orange'; magenta = 'Magenta'; light_blue = 'Light Blue'; yellow = 'Yellow'
    lime = 'Lime'; pink = 'Pink'; gray = 'Gray'; light_gray = 'Light Gray'; cyan = 'Cyan'; purple = 'Purple'
    blue = 'Blue'; brown = 'Brown'; green = 'Green'; red = 'Red'; black = 'Black'
}
$script:ColorNamesZh = @{
    white = '白色'; orange = '橙色'; magenta = '品红色'; light_blue = '淡蓝色'; yellow = '黄色'
    lime = '黄绿色'; pink = '粉红色'; gray = '灰色'; light_gray = '淡灰色'; cyan = '青色'; purple = '紫色'
    blue = '蓝色'; brown = '棕色'; green = '绿色'; red = '红色'; black = '黑色'
}

# ---- texture ----------------------------------------------------------------------------------------

function New-Sheet([int]$w = 48, [int]$h = 32) {
    $g = New-Object 'char[][]' $h
    for ($y = 0; $y -lt $h; $y++) { $g[$y] = ('.' * $w).ToCharArray() }
    return , $g
}

# Deterministic noise in 0..99 for a pixel (same stone pattern for every colour).
function Get-Noise([int]$x, [int]$y, [int]$seed) {
    $v = ($x * 73856093) -bxor ($y * 19349663) -bxor ($seed * 83492791)
    $v = ($v -bxor ($v -shr 13)) * 1274126177
    return [int]([Math]::Abs($v % 100))
}

# Fills rows y0..y1 (inclusive) of columns x0..x1 with a style:
#   brick     stone bricks: mortar rows every 4 rows, staggered joints, speckles
#   polished  smooth polished stone, highlight on the first row
#   dark      darker polished band with a shadow line at the bottom
#   trim      cap: highlight row, light stone, dark edge at the bottom
function Fill-Band($g, [int]$x0, [int]$x1, [int]$y0, [int]$y1, [string]$style, [int]$seed = 1) {
    for ($y = $y0; $y -le $y1; $y++) {
        for ($x = $x0; $x -le $x1; $x++) {
            $n = Get-Noise $x $y $seed
            $c = 'b'
            switch ($style) {
                'brick' {
                    $row = ($y - $y0) % 4
                    $course = [Math]::Floor(($y - $y0) / 4)
                    $joint = if ($course % 2 -eq 0) { 3 } else { 11 }
                    if ($row -eq 3) { $c = 'a' }
                    elseif (($x % 16) -eq $joint) { $c = 'a' }
                    elseif ($row -eq 0) { $c = if ($n -lt 70) { 'c' } else { 'd' } }
                    elseif ($n -lt 18) { $c = 'n' } elseif ($n -lt 40) { $c = 'c' } else { $c = 'b' }
                }
                'polished' {
                    if ($y -eq $y0) { $c = 'd' } elseif ($n -lt 15) { $c = 'e' } else { $c = 'f' }
                }
                'dark' {
                    if ($y -eq $y1) { $c = 'a' } elseif ($n -lt 25) { $c = 'f' } else { $c = 'e' }
                }
                'trim' {
                    if ($y -eq $y0) { $c = 'd' } elseif ($y -eq $y1) { $c = 'a' } elseif ($n -lt 20) { $c = 'b' } else { $c = 'c' }
                }
            }
            $g[$y][$x] = [char]$c
        }
    }
}

# Draws rune rows (strings of '.', 'r', 'u' = bright) at (x, y); each rune pixel gets an engraved shadow
# ('q') below it when that pixel is plain stone.
function Draw-Runes($g, [int]$x, [int]$y, [string[]]$rows) {
    for ($j = 0; $j -lt $rows.Count; $j++) {
        for ($i = 0; $i -lt $rows[$j].Length; $i++) {
            $ch = $rows[$j][$i]
            if ($ch -ne '.') { $g[$y + $j][$x + $i] = $ch }
        }
    }
    for ($j = 0; $j -lt $rows.Count; $j++) {
        for ($i = 0; $i -lt $rows[$j].Length; $i++) {
            if ($rows[$j][$i] -eq '.') { continue }
            $sx = $x + $i; $sy = $y + $j + 1
            if ($sy -lt $g.Count -and $sx -lt $g[$sy].Length -and 'bcdn' -match [regex]::Escape([string]$g[$sy][$sx])) { $g[$sy][$sx] = 'q' }
        }
    }
}

function Draw-Gem($g, [int]$x, [int]$y) {
    $rows = @('ihhh', 'hhhg', 'hhgg', 'hggk')
    for ($j = 0; $j -lt 4; $j++) { for ($i = 0; $i -lt 4; $i++) { $g[$y + $j][$x + $i] = $rows[$j][$i] } }
}

function Get-StonePalette([string]$color) {
    $rune = ConvertTo-Color $script:DyeDiffuse[$color]
    return @{
        a = '#4a4b4f'; b = '#7d7e83'; c = '#8f9095'; d = '#a9aaaf'; n = '#6f7075'
        e = '#55585c'; f = '#686b70'
        r = $rune; u = (Shade-Color $rune 1.45); q = (Mix-Color (Shade-Color $rune 0.4) '#3a3b3f' 0.75)
        g = '#5b2a86'; h = '#8a4fcf'; i = '#c9a2ff'; k = '#3e1a5e'
    }
}

function Save-Sheet($g, [string]$color, [string]$path) {
    $rows = foreach ($line in $g) { -join $line }
    Save-PixelArt -Rows $rows -Palette (Get-StonePalette $color) -Path $path
}

# ---- geometry ----------------------------------------------------------------------------------------

# A box in block pixel coordinates (0..16) of one half. Optional: rot (x,y,z), pivot (block
# coordinates), faceUv = @{ up = @(u, v, w, h); ... } overrides, allUv = @(u, v, w, h).
function New-Box([double]$x, [double]$y, [double]$z, [double]$w, [double]$h, [double]$d, $extra = @{}) {
    $b = @{ x = $x; y = $y; z = $z; w = $w; h = $h; d = $d }
    foreach ($k in $extra.Keys) { $b[$k] = $extra[$k] }
    return $b
}

function Get-FaceUv($box, [int]$half) {
    $row = if ($half -eq 1) { 0 } else { 16 }
    $v = $row + 16 - ($box.y + $box.h)
    $uv = [ordered]@{
        north = [ordered]@{ uv = @($box.x, $v); uv_size = @($box.w, $box.h) }
        south = [ordered]@{ uv = @($box.x, $v); uv_size = @($box.w, $box.h) }
        east  = [ordered]@{ uv = @($box.z, $v); uv_size = @($box.d, $box.h) }
        west  = [ordered]@{ uv = @($box.z, $v); uv_size = @($box.d, $box.h) }
        up    = [ordered]@{ uv = @((16 + $box.x), $box.z); uv_size = @($box.w, $box.d) }
        down  = [ordered]@{ uv = @((16 + $box.x), (16 + $box.z)); uv_size = @($box.w, $box.d) }
    }
    if ($box.allUv) {
        foreach ($f in @($uv.Keys)) { $uv[$f] = [ordered]@{ uv = @($box.allUv[0], $box.allUv[1]); uv_size = @($box.allUv[2], $box.allUv[3]) } }
    }
    if ($box.faceUv) {
        foreach ($f in $box.faceUv.Keys) { $o = $box.faceUv[$f]; $uv[$f] = [ordered]@{ uv = @($o[0], $o[1]); uv_size = @($o[2], $o[3]) } }
    }
    return $uv
}

# Bedrock cube: origin relative to the block centre (x/z - 8), lifted by $yOffset and scaled.
function ConvertTo-Cube($box, [int]$half, [double]$yOffset = 0, [double]$scale = 1) {
    $cube = [ordered]@{
        origin = @((($box.x - 8) * $scale), (($box.y + $yOffset) * $scale), (($box.z - 8) * $scale))
        size   = @(($box.w * $scale), ($box.h * $scale), ($box.d * $scale))
    }
    if ($box.rot) {
        $cube.pivot    = @((($box.pivot[0] - 8) * $scale), (($box.pivot[1] + $yOffset) * $scale), (($box.pivot[2] - 8) * $scale))
        $cube.rotation = $box.rot
    }
    $cube.uv = Get-FaceUv $box $half
    return $cube
}

function New-Geometry([string]$id, $cubes) {
    return [ordered]@{
        description = [ordered]@{
            identifier = $id; texture_width = 48; texture_height = 32
            visible_bounds_width = 2; visible_bounds_height = 3; visible_bounds_offset = @(0, 1, 0)
        }
        bones = @([ordered]@{ name = 'stone'; pivot = @(0, 0, 0); cubes = @($cubes) })
    }
}

# Writes models/blocks/<name>.geo.json with geometry.bedstones.<name>_lower / _upper / _item.
function Save-StoneGeometry([string]$path, [string]$name, $lowerBoxes, $upperBoxes) {
    $lower = foreach ($b in $lowerBoxes) { ConvertTo-Cube $b 0 }
    $upper = foreach ($b in $upperBoxes) { ConvertTo-Cube $b 1 }
    # The item icon: both halves stacked and scaled to one block.
    $item = @(foreach ($b in $lowerBoxes) { ConvertTo-Cube $b 0 0 0.5 }) + @(foreach ($b in $upperBoxes) { ConvertTo-Cube $b 1 16 0.5 })
    $doc = [ordered]@{
        format_version     = '1.16.0'
        'minecraft:geometry' = @(
            (New-Geometry "geometry.bedstones.$($name)_lower" $lower),
            (New-Geometry "geometry.bedstones.$($name)_upper" $upper),
            (New-Geometry "geometry.bedstones.$($name)_item" $item)
        )
    }
    Save-Json $doc $path
}

function Save-Json($obj, [string]$path) {
    $dir = Split-Path -Parent $path
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    $json = $obj | ConvertTo-Json -Depth 30
    [IO.File]::WriteAllText($path, $json + "`n", [Text.UTF8Encoding]::new($false))
}

function Save-Text([string]$text, [string]$path) {
    $dir = Split-Path -Parent $path
    if ($dir) { New-Item -ItemType Directory -Force $dir | Out-Null }
    [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
}

# ---- block JSON ----------------------------------------------------------------------------------------

function Get-Box([double[]]$o, [double[]]$s) { return [ordered]@{ origin = $o; size = $s } }

# Behavior pack block of a two-tall dyed stone. $lowerBox / $upperBox: collision = selection boxes.
function New-StoneBlock([string]$path, [string]$model, [string]$texture, $lowerBox, $upperBox) {
    $material = [ordered]@{ '*' = [ordered]@{ texture = $texture; render_method = 'alpha_test' } }
    $rotations = @{ 0 = 180; 1 = 90; 2 = 0; 3 = 270 } # bedstones:facing -> y rotation (model front = north)
    $perms = @(foreach ($f in 0..3) {
            [ordered]@{
                condition  = "q.block_state('bedstones:facing') == $f"
                components = [ordered]@{ 'minecraft:transformation' = [ordered]@{ rotation = @(0, $rotations[$f], 0) } }
            }
        })
    $perms += [ordered]@{
        condition  = "q.block_state('bedstones:upper') == 1"
        components = [ordered]@{
            'minecraft:geometry'      = "geometry.bedstones.$($model)_upper"
            'minecraft:collision_box' = $upperBox
            'minecraft:selection_box' = $upperBox
        }
    }
    return [ordered]@{
        format_version  = '1.26.0'
        'minecraft:block' = [ordered]@{
            description = [ordered]@{
                identifier    = "bedstones:$path"
                menu_category = [ordered]@{ category = 'construction' }
                states        = [ordered]@{ 'bedstones:facing' = @(0, 1, 2, 3); 'bedstones:upper' = @(0, 1) }
            }
            components = [ordered]@{
                'minecraft:display_name'               = "tile.bedstones:$path.name"
                'minecraft:geometry'                   = "geometry.bedstones.$($model)_lower"
                'minecraft:material_instances'         = $material
                'minecraft:item_visual'                = [ordered]@{ geometry = "geometry.bedstones.$($model)_item"; material_instances = $material }
                'minecraft:collision_box'              = $lowerBox
                'minecraft:selection_box'              = $lowerBox
                'minecraft:destructible_by_mining'     = [ordered]@{ seconds_to_destroy = 5 }
                'minecraft:destructible_by_explosion'  = [ordered]@{ explosion_resistance = 2000 }
                'minecraft:movable'                    = [ordered]@{ movement_type = 'immovable' }
                # Drops are made by the mod (survival pickaxe breaks of either half, docs/SPEC.md §1.1).
                'minecraft:loot'                       = 'loot_tables/blocks/bedstones_stones_none.json'
                'minecraft:light_dampening'            = 0
                'minecraft:map_color'                  = '#7d7e83'
                'tag:stone'                            = @{}
                'tag:minecraft:is_pickaxe_item_destructible' = @{}
            }
            permutations = $perms
        }
    }
}

function New-ShapedRecipe([string]$path, [string[]]$pattern, $key) {
    return [ordered]@{
        format_version          = '1.20.10'
        'minecraft:recipe_shaped' = [ordered]@{
            description = [ordered]@{ identifier = "bedstones:$path" }
            tags        = @('crafting_table')
            pattern     = $pattern
            key         = $key
            unlock      = @([ordered]@{ item = 'bedstones:warp_stone' })
            result      = [ordered]@{ item = "bedstones:$path" }
        }
    }
}
