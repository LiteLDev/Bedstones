# U17 client visuals: rune glow (activation-dependent, style tint), ambient particles, sharestone
# runes + floating warp stone, and `client.disableTextGlow`.
# Stones come from `/waystones visuals place <kind>` (the real block when a unit registered it,
# else a stone brick wall stand-in) 3 blocks in front of the player.
# Screenshots: out/visuals/{before,activated,activated_close,forgotten,sharestone_a,sharestone_b,
#                           night_glow,night_lit}.png

function Snap([string]$name, [int]$settleMs = 1500) {
    Press-Key F1 # hide the HUD (chat lines, hotbar)
    Start-Sleep -Milliseconds $settleMs
    Save-Screenshot $name | Out-Null
    Press-Key F1
}

$r = Invoke-SelfTest 'visuals.*'
if ($r.Pass -lt 4) { throw "expected 4 visuals selftests to pass, got $($r.Pass)" }

Wait-LogLine 'companion: server ready' 30 | Out-Null
Wait-LogLine 'visuals: client has' 30 | Out-Null
Reset-CameraPerspective
Invoke-Chat '/gamerule dodaylightcycle false'
Invoke-Chat '/time set noon'
Invoke-Chat '/weather clear'
Invoke-Chat '/waystones visuals clear'

# Three waystones in a row, facing south: default (white runes), Black (0x993333), Ender (0x7200FF).
Invoke-Chat '/tp @s ~ ~ ~ 0 0'
$mark = Get-LogMark
Invoke-Chat '/waystones visuals place waystone'
Invoke-Chat '/tp @s ~2 ~ ~'
Invoke-Chat '/waystones visuals place blackstone_waystone'
Invoke-Chat '/tp @s ~-4 ~ ~'
Invoke-Chat '/waystones visuals place end_stone_waystone'
Wait-LogLine 'visuals: placed end_stone_waystone' 10 -Since $mark | Out-Null
Wait-LogLine 'visuals: client has 3 waystones \(0 with runes\)' 10 -Since $mark | Out-Null
Invoke-Chat '/tp @s ~2 ~0.5 ~-1 0 12'
Snap 'before'

# Activate them through the record API: runes glow, ambient particles appear.
$mark = Get-LogMark
Invoke-Chat '/waystones visuals activate'
Wait-LogLine 'visuals: activated 3 waystones' 10 -Since $mark | Out-Null
Wait-LogLine 'visuals: client has 3 waystones \(3 with runes\)' 10 -Since $mark | Out-Null
Wait-LogLine 'visuals: drawing runes of blackstone_waystone .* \(colour 993333\)' 10 -Since $mark | Out-Null
Wait-LogLine 'visuals: drawing runes of end_stone_waystone .* \(colour 7200FF\)' 10 -Since $mark | Out-Null
Wait-LogLine 'visuals: drawing runes of waystone .* \(colour FFFFFF\)' 10 -Since $mark | Out-Null
Snap 'activated'
Invoke-Chat '/tp @s ~ ~ ~1.5 0 20'
Snap 'activated_close'

# Forget them again: runes and particles go away.
$mark = Get-LogMark
Invoke-Chat '/waystones visuals forget'
Wait-LogLine 'visuals: client has 3 waystones \(0 with runes\)' 10 -Since $mark | Out-Null
Snap 'forgotten'

# A red sharestone: runes always (dye colour B02E26) and the floating, spinning warp stone.
$mark = Get-LogMark
Invoke-Chat '/waystones visuals clear'
Invoke-Chat '/tp @s ~ ~ ~-1.5 0 0'
Invoke-Chat '/waystones visuals place red_sharestone'
Wait-LogLine 'visuals: drawing runes of red_sharestone .* \(colour B02E26\)' 10 -Since $mark | Out-Null
Invoke-Chat '/tp @s ~ ~ ~0.5 0 -5'
Snap 'sharestone_a'
Start-Sleep -Seconds 4
Snap 'sharestone_b' 200

# Night with the glow variant, then with client.disableTextGlow (the lit variant). In 1.26.51
# every particle takes the world light, so the two look alike (documented deviation); this checks
# that the config switch and reload work without errors.
$mark = Get-LogMark
Invoke-Chat '/tp @s ~2 ~ ~-0.5 0 0'
Invoke-Chat '/waystones visuals place waystone'
Invoke-Chat '/tp @s ~-1 ~ ~ 0 5'
Invoke-Chat '/waystones visuals activate'
Wait-LogLine 'visuals: client has 2 waystones \(2 with runes\)' 10 -Since $mark | Out-Null
Invoke-Chat '/time set midnight'
Snap 'night_glow' 2500
$cfg = Join-Path $script:ModsDir 'Bedstones\config\client.json'
$json = Get-Content $cfg -Raw | ConvertFrom-Json
$json.disableTextGlow = $true
$json | ConvertTo-Json | Set-Content $cfg
Invoke-Chat '/waystones reload'
Snap 'night_lit' 2500
$json.disableTextGlow = $false
$json | ConvertTo-Json | Set-Content $cfg
Invoke-Chat '/waystones reload'
Invoke-Chat '/time set noon'
Invoke-Chat '/waystones visuals clear'
