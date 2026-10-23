# Draws the Twinbound Feather item texture (16x16), original pixel art: a diagonal feather whose
# vane is split in two halves (pale white and amethyst violet, the "twins"), bound by a gold band
# on an ink-dark quill (the recipe's feather, amethyst shard, gold nugget and ink sac).
. "$PSScriptRoot/lib.ps1"

# o outline, w white vane, s white shade, a amethyst vane, d amethyst shade, l amethyst light,
# g gold band, h gold shade, q quill (ink), k quill dark
$Rows = @(
    '...........oooo.',
    '.........ooawwlo',
    '........oaawwwwo',
    '.......oadawwwso',
    '......oaadawwso.',
    '.....oadaawwwso.',
    '....oaadawwwso..',
    '...oadaawwsso...',
    '...odaaqwsso....',
    '..oddaqwsso.....',
    '..oddqssoo......',
    '..oohgoo........',
    '..oghho.........',
    '.oqkoo..........',
    'oqko............',
    'oko.............'
)
$Palette = @{
    o = '#1c1426'
    w = '#f4f1fa'
    s = '#c9c2d8'
    a = '#a77be0'
    d = '#7448b4'
    l = '#d9c2ff'
    g = '#f2c84b'
    h = '#b8862a'
    q = '#3a3550'
    k = '#221f30'
}

$root = (Resolve-Path "$PSScriptRoot/../..").Path
$path = Join-Path $root 'assets/pack/rp/textures/items/twinbound_feather.png'
Save-PixelArt -Rows $Rows -Palette $Palette -Path $path
Write-Host "wrote $path"
