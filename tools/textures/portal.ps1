# U5 textures: Portal Scroll, Epitaph, Warp Portal (animated flipbook), Fleeting Memorial and the
# particle sheet used by bedstones:reverse_portal / bedstones:memorial_ash. Original pixel art.
#
#   pwsh tools/textures/portal.ps1 [-Preview]
param([switch]$Preview)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/lib.ps1"
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$rp = Join-Path $root 'assets/pack/rp/textures'
$scale = if ($Preview) { 8 } else { 1 }

# ---- Portal Scroll: a rolled parchment tied with a violet ribbon and an amethyst seal --------------
$scroll = @(
    '................',
    '..........kkk...',
    '.........kwwwk..',
    '........kwppwk..',
    '.......kpppwk...',
    '......kpppwk....',
    '.....kpppwk.....',
    '....kpvvvk......',
    '...kpvaavk......',
    '..kppvaavk......',
    '.kpppkvvk.......',
    'kwpppk..........',
    'kwwpk...........',
    '.kkk............',
    '................',
    '................'
)
$scrollPal = @{
    k = '#3b2a1e'; w = '#f4e9c8'; p = '#d9c48f'; v = '#7b3fb0'; a = '#c58cf2'
}
Save-PixelArt -Rows $scroll -Palette $scrollPal -Path "$rp/items/portal_scroll.png" -Scale $scale

# ---- Epitaph: a small slate tablet with gold corners and an amethyst inlay ----------------------
$epitaph = @(
    '................',
    '.....kkkkkk.....',
    '....kssssssk....',
    '...kssllllssk...',
    '..kgsllsslllgk..',
    '..ksllsaaslllk..',
    '..kslsabbasllk..',
    '..ksllsaaslllk..',
    '..kslllsslllsk..',
    '..ksldddddllsk..',
    '..kslllllllssk..',
    '..ksldddddlssk..',
    '..kgsssssssssk..',
    '..kggkkkkkkggk..',
    '...kk......kk...',
    '................'
)
$epitaphPal = @{
    k = '#1f1d24'; s = '#4a4752'; l = '#5f5b68'; d = '#3a3740'; g = '#e8b923'; a = '#9a5cd6'; b = '#d9a8ff'
}
Save-PixelArt -Rows $epitaph -Palette $epitaphPal -Path "$rp/items/epitaph.png" -Scale $scale

# ---- Warp Portal: 8 frames of a slowly turning violet swirl (16 x 128 flipbook) -----------------
$shades = @('#2a0d4a', '#3d1468', '#521d86', '#6a29a3', '#8338bd', '#9d52d4', '#b877e6', '#d6a8f5')
$portalPal = @{}
for ($i = 0; $i -lt 8; $i++) {
    # Slightly translucent so the world shows through the edges.
    $c = ConvertTo-Color $shades[$i]
    $alpha = 170 + $i * 10
    $portalPal["$i"] = [System.Drawing.Color]::FromArgb($alpha, $c.R, $c.G, $c.B)
}
$rows = New-Object System.Collections.Generic.List[string]
for ($f = 0; $f -lt 8; $f++) {
    $phase = $f * [Math]::PI * 2 / 8
    for ($y = 0; $y -lt 16; $y++) {
        $line = ''
        for ($x = 0; $x -lt 16; $x++) {
            $dx = $x - 7.5; $dy = $y - 7.5
            $r = [Math]::Sqrt($dx * $dx + $dy * $dy)
            $a = [Math]::Atan2($dy, $dx)
            $v = [Math]::Sin($r * 0.9 - $a * 2 + $phase) * 0.5 + 0.5
            $v = $v * 0.75 + [Math]::Sin(($x * 3 + $y * 5 + $f * 7) * 0.7) * 0.12 + 0.12
            $idx = [int][Math]::Floor([Math]::Max([double]0.0, [Math]::Min([double]0.999, [double]$v)) * 8)
            $line += "$idx"
        }
        $rows.Add($line)
    }
}
Save-PixelArt -Rows $rows.ToArray() -Palette $portalPal -Path "$rp/blocks/warp_portal.png" -Scale $scale

# ---- Fleeting Memorial: 32 x 16, left = engraved front, right = plain weathered stone -----------
$front = @(
    'ssssssssssssssss',
    'sssmmmmmmmmmmsss',
    'ssmlllllllllllms',
    'ssmlllllllllllms',
    'ssmllllddlllllms',
    'ssmllllddlllllms',
    'ssmlldddddlllllm',
    'ssmlldddddlllllm',
    'ssmllllddllllllm',
    'ssmllllddllllllm',
    'ssmllllddllllllm',
    'ssmlllllllllllms',
    'ssmlllllllllllms',
    'sssmmmmmmmmmmsss',
    'ssssssssssssssss',
    'ssssssssssssssss'
)
$plain = for ($y = 0; $y -lt 16; $y++) {
    $line = ''
    for ($x = 0; $x -lt 16; $x++) {
        $n = (($x * 7 + $y * 13 + ($x * $y) % 5) % 9)
        $line += if ($n -eq 0) { 'd' } elseif ($n -lt 3) { 'm' } else { 'l' }
    }
    $line
}
$memorial = for ($y = 0; $y -lt 16; $y++) { $front[$y] + $plain[$y] }
$memorialPal = @{ s = '#4b4853'; m = '#5c5966'; l = '#6d6a78'; d = '#2f2d35' }
Save-PixelArt -Rows $memorial -Palette $memorialPal -Path "$rp/blocks/fleeting_memorial.png" -Scale $scale

# ---- Particle sheet: [0,0] soft round dot (portal), [4,0] small speck (ash) ---------------------
$particle = @(
    '.ww..xx.........',
    'wwwwxxxx........',
    'wwwwxxxx........',
    '.ww..xx.........',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................',
    '................'
)
Save-PixelArt -Rows $particle -Palette @{ w = '#ffffff'; x = '#e0e0e0' } -Path "$rp/particle/bedstones_particles.png" -Scale $scale

Write-Host 'portal textures written'
