# U17 client visuals textures (original pixel art):
#   textures/bs_visuals/runes.png                 24x16 rune atlas: 8x16 cells (waystone, sharestone,
#                                                 portstone), white so the particle tint gives the colour
#   textures/bs_visuals/floating_warp_stone.png   16x16 sprite of the warp stone floating above sharestones
#
# Run: pwsh tools/textures/visuals.ps1   (also writes x8 previews to $env:TEMP when -Preview)
param([switch]$Preview)
. "$PSScriptRoot/lib.ps1"

$root = Join-Path $PSScriptRoot '../../assets/pack/rp/textures/bs_visuals'

# Each cell: three 6x4 glyphs with one blank row around them, a 1-pixel margin left and right.
# W = rune stroke, g = softer stroke (the tint multiplies both).
$waystone = @(
    '........',
    '..W..W..',
    '..WWWW..',
    '....W...',
    '...WW...',
    '........',
    '.W....W.',
    '.gW..Wg.',
    '...WW...',
    '..W..W..',
    '........',
    '..WWW...',
    '.W...W..',
    '..WWW...',
    '....W...',
    '........'
)
$sharestone = @(
    '........',
    '.WWWWW..',
    '...W....',
    '..WgW...',
    '.W...W..',
    '........',
    '..W.W...',
    '..WWWW..',
    '..W.W...',
    '..WWWW..',
    '........',
    '.W...W..',
    '..W.W...',
    '...W....',
    '.WWWWW..',
    '........'
)
$portstone = @(
    '........',
    '...W....',
    '..WWW...',
    '.W.W.W..',
    '...W....',
    '........',
    '.WW.WW..',
    '..W.W...',
    '..W.W...',
    '.WW.WW..',
    '........',
    '..W.....',
    '..WWWW..',
    '.....W..',
    '..WWWW..',
    '........'
)
$atlas = for ($y = 0; $y -lt 16; $y++) { $waystone[$y] + $sharestone[$y] + $portstone[$y] }
$runePal = @{ W = '#ffffff'; g = '#c8c8c8' }
Save-PixelArt -Rows $atlas -Palette $runePal -Path (Join-Path $root 'runes.png')

# Warp stone: a faceted violet crystal with a bright core.
$stone = @(
    '................',
    '.......oo.......',
    '......oLLo......',
    '.....oLHLMo.....',
    '....oLHLMMMo....',
    '...oLHLMMMMDo...',
    '..oLLLMMMMMDDo..',
    '..oLMMMCCMMDDo..',
    '..oMMMCWWCMDDo..',
    '..oMMMMCCMMDDo..',
    '...oMMMMMMDDo...',
    '....oMMMMDDo....',
    '.....oMMDDo.....',
    '......oDDo......',
    '.......oo.......',
    '................'
)
$stonePal = @{ o = '#2a1040'; L = '#d9a8ff'; H = '#f4e4ff'; M = '#9b4fd6'; D = '#5e2794'; C = '#c58bff'; W = '#ffffff' }
Save-PixelArt -Rows $stone -Palette $stonePal -Path (Join-Path $root 'floating_warp_stone.png')

if ($Preview) {
    Save-PixelArt -Rows $atlas -Palette $runePal -Path (Join-Path $env:TEMP 'bs_runes_preview.png') -Scale 8
    Save-PixelArt -Rows $stone -Palette $stonePal -Path (Join-Path $env:TEMP 'bs_floating_preview.png') -Scale 8
}
