# Draws U4's item textures (16x16, original pixel art): the Warp Stone (an amethyst crystal with
# an emerald heart, from its recipe) and four parchment scrolls with gold-capped rolls (gold
# nuggets + paper), each marked with its own emblem:
#   warp_scroll   ender-pearl eye        return_scroll  a small house (home)
#   blank_scroll  plain parchment        bound_scroll   a waystone with an aqua rune
. "$PSScriptRoot/lib.ps1"

$root = (Resolve-Path "$PSScriptRoot/../..").Path
$dir  = Join-Path $root 'assets/pack/rp/textures/items'
New-Item -ItemType Directory -Force $dir | Out-Null

# ---- scrolls ------------------------------------------------------------------------------
# o outline, g gold cap, R roll, S roll shade, P parchment, Q parchment shade
$Scroll = @(
    '................',
    '..oooooooooooo..',
    '.gRRRRRRRRRRRRg.',
    '.gSSSSSSSSSSSSg.',
    '..oPPPPPPPPPQo..',
    '..oPPPPPPPPPQo..',
    '..oPPPPPPPPPQo..',
    '..oPPPPPPPPPQo..',
    '..oPPPPPPPPPQo..',
    '..oPPPPPPPPPQo..',
    '..oPPPPPPPPPQo..',
    '..oPPPPPPPPPQo..',
    '.gRRRRRRRRRRRRg.',
    '.gSSSSSSSSSSSSg.',
    '..oooooooooooo..',
    '................'
)
$ScrollPalette = @{
    o = '#4a3420'; g = '#e6b422'; R = '#e8dcb8'; S = '#b8a47c'; P = '#f3ead2'; Q = '#d9caa3'
}

function Emblem([string[]]$Lines) {
    # $Lines are rows 5..10 of the texture; the rest stays transparent.
    $empty = '................'
    $rows = @($empty, $empty, $empty, $empty, $empty) + $Lines
    while ($rows.Count -lt 16) { $rows += $empty }
    return , $rows
}

$Scrolls = @{
    blank_scroll  = @{ Emblem = $null; Colors = @{} }
    warp_scroll   = @{
        # m pearl rim, c pearl, w highlight
        Emblem = Emblem @(
            '.......mm.......',
            '......mccm......',
            '.....mcwwcm.....',
            '.....mcwwcm.....',
            '......mccm......',
            '.......mm.......')
        Colors = @{ m = '#123b36'; c = '#2a8f7e'; w = '#9ff0dc' }
    }
    return_scroll = @{
        # h roof, b wall, d door
        Emblem = Emblem @(
            '.......hh.......',
            '......hhhh......',
            '.....hhhhhh.....',
            '......bddb......',
            '......bddb......',
            '......bbbb......')
        Colors = @{ h = '#2f7d4a'; b = '#8a6a44'; d = '#3b2a1a' }
    }
    bound_scroll  = @{
        # t stone, k stone shade, a aqua rune
        Emblem = Emblem @(
            '.......tt.......',
            '......tkkt......',
            '......tkat......',
            '......takt......',
            '......tkkt......',
            '.....tttttt.....')
        Colors = @{ t = '#6f6f6f'; k = '#4a4a4a'; a = '#3fe0e8' }
    }
}

foreach ($name in $Scrolls.Keys) {
    $def  = $Scrolls[$name]
    $rows = if ($def.Emblem) { Merge-Rows $Scroll $def.Emblem } else { $Scroll }
    $pal  = $ScrollPalette.Clone()
    foreach ($k in $def.Colors.Keys) { $pal[$k] = $def.Colors[$k] }
    $path = Join-Path $dir "$name.png"
    Save-PixelArt -Rows $rows -Palette $pal -Path $path
    Write-Host "wrote $path"
}

# ---- warp stone ---------------------------------------------------------------------------
# o outline, L light amethyst, w glint, M amethyst, D dark amethyst, e emerald shade, n emerald
$Stone = @(
    '................',
    '................',
    '......oooo......',
    '.....oLLLLo.....',
    '....oLwLLLMo....',
    '...oLwLLLMMMo...',
    '..oLwLLeeMMMDo..',
    '..oLLLenneMMDo..',
    '..oLLMenneMDDo..',
    '..oLMMMeeMMDDo..',
    '...oMMMMMMDDo...',
    '....oMMMDDDo....',
    '.....oMDDDo.....',
    '......oooo......',
    '................',
    '................'
)
$StonePalette = @{
    o = '#2a1240'; L = '#d8a8ff'; w = '#ffffff'; M = '#9b5bd6'; D = '#5e2f8f'; e = '#1f9e6a'; n = '#56e39f'
}
$path = Join-Path $dir 'warp_stone.png'
Save-PixelArt -Rows $Stone -Palette $StonePalette -Path $path
Write-Host "wrote $path"
