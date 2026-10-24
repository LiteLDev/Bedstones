# Font glyph page with a Standard-Galactic-style alphabet (original pixel art), used for warp plate
# names (Waystones shows them in the enchanting-table "alt" font).
#
#   pwsh tools/textures/galactic.ps1
#
# Bedrock loads resource-pack glyph pages font/glyph_XX.png (256x256, 16x16 cells, code points
# U+XX00..U+XXFF; the glyph's width is its non-transparent extent). Letter 'a'..'z' maps to
# U+E161..U+E17A (the ASCII position in page E1), see core/warpplate/WarpPlateLogic.h (galacticText).
. "$PSScriptRoot/lib.ps1"

# 5x7 glyphs ('#' = ink), drawn 2x into the 16x16 cell.
$glyphs = [ordered]@{
    a = @('#....', '#....', '#....', '#####', '.....', '.....', '.....')
    b = @('..#..', '..#..', '..#..', '..#..', '#...#', '.###.', '.....')
    c = @('#####', '#....', '#....', '#..#.', '#....', '#####', '.....')
    d = @('#####', '#....', '#.#..', '#..#.', '#...#', '.....', '.....')
    e = @('#...#', '#...#', '#####', '..#..', '..#..', '..#..', '.....')
    f = @('#####', '.....', '#####', '.....', '#...#', '#...#', '.....')
    g = @('#####', '....#', '....#', '..###', '....#', '....#', '.....')
    h = @('#####', '..#..', '..#..', '.#.#.', '.#.#.', '.#.#.', '.....')
    i = @('#....', '.....', '#....', '#....', '.....', '#....', '.....')
    j = @('#....', '.....', '#....', '.....', '#....', '.....', '.....')
    k = @('#...#', '#..#.', '###..', '#..#.', '#...#', '.....', '.....')
    l = @('#....', '#....', '#....', '#....', '#####', '..#..', '.....')
    m = @('#...#', '#...#', '#.#.#', '#.#.#', '#####', '.....', '.....')
    n = @('#..#.', '#..#.', '#..#.', '#..#.', '...#.', '..#..', '.....')
    o = @('#####', '....#', '....#', '#####', '#....', '#####', '.....')
    p = @('#.#..', '#.#..', '#.#..', '.....', '#.#..', '#.#..', '.....')
    q = @('#####', '#...#', '#...#', '#...#', '.....', '.....', '.....')
    r = @('#.#..', '.....', '#.#..', '.....', '#.#..', '.....', '.....')
    s = @('#####', '#...#', '.....', '#####', '....#', '#####', '.....')
    t = @('#####', '....#', '....#', '....#', '....#', '.....', '.....')
    u = @('.#...', '#.#..', '.#...', '.....', '.#...', '#.#..', '.....')
    v = @('..#..', '.....', '#####', '#...#', '#...#', '.....', '.....')
    w = @('..#..', '.....', '.....', '#...#', '.....', '.....', '.....')
    x = @('....#', '...#.', '..#..', '.#...', '#....', '.....', '.....')
    y = @('#.#..', '#.#..', '#.#..', '#.#..', '#.#..', '.....', '.....')
    z = @('#####', '#...#', '#...#', '.....', '..#..', '.....', '.....')
}

$size = 256
$grid = for ($y = 0; $y -lt $size; $y++) { ,([char[]]('.' * $size)) }
foreach ($letter in $glyphs.Keys) {
    $code = [int][char]$letter          # 0x61..0x7A -> cell (row = code >> 4, col = code & 15)
    $cx   = ($code -band 15) * 16
    $cy   = ($code -shr 4) * 16
    $rows = $glyphs[$letter]
    for ($gy = 0; $gy -lt $rows.Count; $gy++) {
        for ($gx = 0; $gx -lt $rows[$gy].Length; $gx++) {
            if ($rows[$gy][$gx] -ne '#') { continue }
            for ($dy = 0; $dy -lt 2; $dy++) {
                for ($dx = 0; $dx -lt 2; $dx++) { $grid[$cy + 1 + 2 * $gy + $dy][$cx + 2 * $gx + $dx] = '#' }
            }
        }
    }
}
$out = Join-Path $PSScriptRoot '../../assets/pack/rp/font/glyph_E1.png'
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
Save-PixelArt -Rows ($grid | ForEach-Object { -join $_ }) -Palette @{ '#' = '#ffffff' } -Path $out
Write-Host "galactic glyph page written: $out"
