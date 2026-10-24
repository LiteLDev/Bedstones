# A normal (non-flat) world, so the game generates real villages, and village waystones on every
# village (FREQUENT) instead of the 15% REGULAR chance.
$props = Join-Path $script:SlotDir 'server.properties'
(Get-Content $props) -replace '^level-type=.*$', 'level-type=DEFAULT' | Set-Content $props -Encoding utf8NoBOM
$cfgDir = Join-Path $script:SlotDir "$(Get-ModsDirName $script:BaseDir)\$script:ModName\config"
New-Item -ItemType Directory -Force $cfgDir | Out-Null
@{
    version                         = 1
    wildWaystoneStyle               = 'BIOME'
    chunksBetweenWildWaystones      = 25
    wildWaystonesDimensionAllowList = @('minecraft:overworld', 'minecraft:the_nether', 'minecraft:the_end')
    wildWaystonesDimensionDenyList  = @()
    spawnInVillages                 = 'FREQUENT'
} | ConvertTo-Json | Set-Content (Join-Path $cfgDir 'worldGen.json') -Encoding utf8NoBOM
Write-Step 'village world: level-type DEFAULT, spawnInVillages FREQUENT'
