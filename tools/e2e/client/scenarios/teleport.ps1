# U7 teleport pipeline on the real client: two waystone records ("Teleport A" next to the player,
# "Teleport B" 24 blocks north, facing north), warp the player from A to B, check it lands north
# of B looking north (yaw 180) with portal particles, then show two action-bar errors (source
# waystone out of range, missing target).
# Screenshots: out/teleport/{before,arrival,arrival_settled,error_out_of_range,error_missing}.png
$r = Invoke-SelfTest 'teleport.landing_*'
if ($r.Pass -lt 2) { throw "expected the landing selftests to pass on the client, got $($r.Pass)" }

$mark = Get-LogMark
Invoke-Chat '/waystones debug teleport setup'
$setup = Wait-LogLine 'teleport debug: setup A=\S+ B=\S+' 20 -Since $mark
Write-Step "setup: $setup"
Start-Sleep -Milliseconds 800
Save-Screenshot 'before' | Out-Null

$mark = Get-LogMark
Invoke-Chat '/waystones debug teleport go'
Wait-LogLine 'warp: \S+ -> Teleport B' 20 -Since $mark | Out-Null
Start-Sleep -Milliseconds 250
Save-Screenshot 'arrival' | Out-Null
$at = Wait-LogLine 'teleport debug: at \S+ \S+ \S+ yaw \S+' 20 -Since $mark
Write-Step "arrival: $at"
if ($at -notmatch 'yaw (-?\d+)') { throw "no yaw in '$at'" }
$yaw = [int]$Matches[1]
if ([math]::Abs([math]::Abs($yaw) - 180) -gt 2) { throw "expected to look north (yaw 180), got $yaw" }
Start-Sleep -Milliseconds 1500
Save-Screenshot 'arrival_settled' | Out-Null

# A is 25 blocks behind now: warping "from A" again is refused (dark red action bar).
$mark = Get-LogMark
Invoke-Chat '/waystones debug teleport go'
Wait-LogLine 'warp refused: \S+ -> Teleport B: chat\.waystones\.source_waystone_out_of_range' 20 -Since $mark | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'error_out_of_range' | Out-Null

Start-Sleep -Milliseconds 2500
$mark = Get-LogMark
Invoke-Chat '/waystones debug teleport missing'
Wait-LogLine 'warp refused: \S+ -> \S+: chat\.waystones\.waystone_missing' 20 -Since $mark | Out-Null
Start-Sleep -Milliseconds 300
Save-Screenshot 'error_missing' | Out-Null

Invoke-Chat '/waystones debug teleport clear'
