# U16 admin commands, typed into chat by the (op) player of the local test world:
# place -> activate -> list / count (chat screenshots) -> cooldown -> twinbound -> forget -> gui.
# Screenshots: out/commands/{list_all,count,list_owned,cooldown,errors,gui}.png
$r = Invoke-SelfTest 'commands.*'
if ($r.Pass -lt 8) { throw "expected at least 8 commands selftests to pass, got $($r.Pass)" }

function Show-ChatHistory([string]$name) {
    # The chat screen shows the recent lines (the HUD fades them out).
    Press-Key T; Start-Sleep -Milliseconds 900
    Save-Screenshot $name | Out-Null
    Press-Key ESC; Start-Sleep -Milliseconds 600
}

# Types a command, screenshots the chat line before sending it (debugging aid), sends it.
function Invoke-ChatShot([string]$text, [string]$shot) {
    Focus-Game
    Press-Key T; Start-Sleep -Milliseconds 700
    Type-Text $text
    Start-Sleep -Milliseconds 300
    Save-Screenshot $shot | Out-Null
    Press-Key ENTER; Start-Sleep -Milliseconds 800
}

$mark = Get-LogMark
Invoke-Chat '/waystones place ~3 ~ ~ waystone Command Alpha'
Wait-LogLine 'commands: placed waystone Command Alpha \(waystone\)' 15 -Since $mark | Out-Null
Invoke-ChatShot '/waystones place ~ ~ ~9 mossy_waystone Command Beta' 'typed_place'
Wait-LogLine 'commands: placed waystone Command Beta \(mossy_waystone\)' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones place ~ ~ ~ granite_waystone Nope'

$mark = Get-LogMark
Invoke-Chat '/waystones activate @s ~3 ~ ~'
Wait-LogLine 'commands: activated waystone Command Alpha for 1 players' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones activate @s ~ ~ ~9'
Wait-LogLine 'commands: activated waystone Command Beta for 1 players' 15 -Since $mark | Out-Null

$mark = Get-LogMark
Invoke-Chat '/waystones list @s all'
Wait-LogLine 'commands: list .* all -> 2 entries' 15 -Since $mark | Out-Null
Show-ChatHistory 'list_all'
Invoke-Chat '/waystones count @s'
Wait-LogLine 'commands: count .* -> 2 activated, 0 owned' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones list @s'
Wait-LogLine 'commands: list [^ ]+ -> 0 entries' 15 -Since $mark | Out-Null
Show-ChatHistory 'count'

$mark = Get-LogMark
Invoke-Chat '/waystones cooldown @s set inventory_button 60'
Wait-LogLine 'commands: cooldown set waystones:inventory_button 60s for 1 players' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones cooldown @s add "waystones:inventory_button" 30'
Wait-LogLine 'commands: cooldown add waystones:inventory_button 30s for 1 players' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones cooldown @s reset inventory_button'
Wait-LogLine 'commands: cooldown reset waystones:inventory_button for 1 players' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones cooldown @s reset all'
Wait-LogLine 'commands: cooldown reset all for 1 players' 15 -Since $mark | Out-Null
Show-ChatHistory 'cooldown'

$mark = Get-LogMark
Invoke-Chat '/waystones twinbound @s @s'
Invoke-Chat '/waystones forget @s ~3 ~ ~'
Wait-LogLine 'commands: deactivated waystone Command Alpha for 1 players' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones forget @s all'
Wait-LogLine 'commands: deactivated all waystones for 1 players' 15 -Since $mark | Out-Null
Invoke-Chat '/waystones activate @s ~ ~50 ~'
Show-ChatHistory 'errors'

# gui: the admin selection (U10's screen when present, otherwise the default form).
$mark = Get-LogMark
Invoke-Chat '/waystones activate @s ~3 ~ ~'
Invoke-Chat '/waystones gui @s'
Wait-LogLine 'commands: gui .* opened for ' 15 -Since $mark | Out-Null
Start-Sleep -Milliseconds 1500
Save-Screenshot 'gui' | Out-Null
Close-Screen
